//! Rust bindings for daoBase's SHM interface (libdao).
//!
//! [`Shm`] mirrors the same create / open / set_data / get_data / close
//! shape as the Python (`daoShm.py`), Julia (`dao.jl`) and MATLAB
//! (`daoShm.m`) bindings, built on libdao's primary API - see
//! `daoBase/include/dao.h` for the underlying C functions.
//!
//! See this crate's README.md for a full walkthrough; `src/main.rs` is a
//! small runnable example (`cargo run`).

use std::ffi::CString;
use std::os::raw::c_void;

/// Raw FFI bindings generated from daoBase/include/dao.h by build.rs (via
/// bindgen), so the struct layouts can never drift from the real C header.
/// Most users want the safe [`Shm`] wrapper below instead of this module.
#[allow(non_camel_case_types, non_snake_case, non_upper_case_globals, dead_code)]
pub mod sys {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

/// The SHM data type codes accepted by `atype` (see dao.h's `_DATATYPE_*`
/// defines). Kept as plain literals here (rather than referencing
/// `sys::_DATATYPE_*`) since bindgen's inferred type for a `#define`
/// constant does not always match the `u8` the C functions expect.
#[repr(u8)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum DataType {
    UInt8 = 1,
    Int8 = 2,
    UInt16 = 3,
    Int16 = 4,
    UInt32 = 5,
    Int32 = 6,
    UInt64 = 7,
    Int64 = 8,
    Float = 9,
    Double = 10,
}

/// Ties a Rust element type to its dao `DataType` code, so
/// `Shm::create::<f32>(...)` can infer `atype` instead of taking it as a
/// separate, easy-to-get-wrong argument.
pub trait DaoType {
    const ATYPE: DataType;
}
impl DaoType for u8 { const ATYPE: DataType = DataType::UInt8; }
impl DaoType for i8 { const ATYPE: DataType = DataType::Int8; }
impl DaoType for u16 { const ATYPE: DataType = DataType::UInt16; }
impl DaoType for i16 { const ATYPE: DataType = DataType::Int16; }
impl DaoType for u32 { const ATYPE: DataType = DataType::UInt32; }
impl DaoType for i32 { const ATYPE: DataType = DataType::Int32; }
impl DaoType for u64 { const ATYPE: DataType = DataType::UInt64; }
impl DaoType for i64 { const ATYPE: DataType = DataType::Int64; }
impl DaoType for f32 { const ATYPE: DataType = DataType::Float; }
impl DaoType for f64 { const ATYPE: DataType = DataType::Double; }

/// A handle to a dao SHM image, created with [`Shm::create`] or attached to
/// an existing one with [`Shm::open`]. The SHM is detached (not deleted;
/// see daoShmClose) when this is dropped.
pub struct Shm {
    image: Box<sys::IMAGE>,
}

impl Shm {
    /// Create (or overwrite) a SHM at `name` (e.g. "/tmp/test.im.shm")
    /// with the given shape, holding elements of type `T`.
    pub fn create<T: DaoType>(name: &str, shape: &[u32]) -> Result<Shm, i8> {
        let mut image: Box<sys::IMAGE> = Box::new(unsafe { std::mem::zeroed() });
        let cname = CString::new(name).expect("SHM name must not contain a NUL byte");
        let result = unsafe {
            sys::daoShmCreate(
                &mut *image,
                cname.as_ptr(),
                shape.len() as std::os::raw::c_long,
                // dao.h's `size` parameter isn't const-qualified even
                // though daoShmCreate only reads it (same for `im` in
                // set_data below) - the C API just isn't const-correct
                // here, so the cast away from `*const` is trusted, not
                // unsound.
                shape.as_ptr() as *mut u32,
                T::ATYPE as u8,
                1, // shared
                0, // NBkw
            )
        };
        if result == 0 { Ok(Shm { image }) } else { Err(result as i8) }
    }

    /// Attach to an existing SHM at `name`.
    pub fn open(name: &str) -> Result<Shm, i8> {
        let mut image: Box<sys::IMAGE> = Box::new(unsafe { std::mem::zeroed() });
        let cname = CString::new(name).expect("SHM name must not contain a NUL byte");
        let result = unsafe { sys::daoShmOpen(cname.as_ptr(), &mut *image) };
        if result == 0 { Ok(Shm { image }) } else { Err(result as i8) }
    }

    /// Write `data` as the next frame and release the SHM's semaphores.
    pub fn set_data<T>(&mut self, data: &[T]) -> Result<(), i8> {
        let result = unsafe {
            sys::daoShmSetData(
                &mut *self.image,
                data.as_ptr() as *mut c_void,
                data.len() as u32,
            )
        };
        if result == 0 { Ok(()) } else { Err(result as i8) }
    }

    /// A view of the most recently written frame, reinterpreted as `&[T]`.
    /// The caller is responsible for `T` matching the SHM's element type
    /// (as passed to [`Shm::create`]); this crate has no way to check that
    /// for a SHM opened with [`Shm::open`].
    pub fn get_data<T>(&mut self) -> Result<&[T], i8> {
        let mut ptr: *mut c_void = std::ptr::null_mut();
        let mut idx: u32 = 0;
        let mut cnt0: u64 = 0;
        let result = unsafe {
            sys::daoShmGetData(&mut *self.image, &mut ptr, &mut idx, &mut cnt0)
        };
        if result != 0 {
            return Err(result as i8);
        }
        let nelement = unsafe { (*self.image.md).nelement } as usize;
        Ok(unsafe { std::slice::from_raw_parts(ptr as *const T, nelement) })
    }

    /// Block until semaphore `sem_nb` is posted (a new frame is available).
    pub fn wait_sem(&mut self, sem_nb: i32) -> Result<(), i8> {
        let result = unsafe { sys::daoShmWaitSem(&mut *self.image, sem_nb) };
        if result == 0 { Ok(()) } else { Err(result as i8) }
    }

    /// The SHM's update counter (incremented on every write).
    pub fn counter(&mut self) -> u64 {
        unsafe { sys::daoShmGetCounter(&mut *self.image) }
    }

    /// The SHM's shape, as declared at creation: a `naxis`-length prefix of
    /// `[size_x, size_y, size_z]` (trailing unused entries are 0).
    pub fn shape(&self) -> [u32; 3] {
        unsafe { (*self.image.md).size }
    }
}

impl Drop for Shm {
    fn drop(&mut self) {
        unsafe {
            sys::daoShmClose(&mut *self.image);
        }
    }
}
