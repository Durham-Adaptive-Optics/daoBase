% Small runnable example for daoShm.m.
%
% Run with:
%     example_basic
% (needs daomex compiled - see README.md - and libdao.so on your library
% path, e.g. via daoBase's dao_env.sh)

name = '/tmp/test_matlab.im.shm';

% Create a new 4x4 single-precision SHM (or overwrite an existing one).
writer = daoShm(name, ones(4, 4, 'single'));
fprintf('created %s, counter = %d\n', name, writer.get_counter());

writer.set_data(2 * ones(4, 4, 'single'));
fprintf('wrote a frame, counter = %d\n', writer.get_counter());

% Attach to the same SHM from a second handle, as a separate process
% reading it would.
reader = daoShm(name);
data = reader.get_data();
fprintf('read back size=[%d %d], sum=%g\n', size(data, 1), size(data, 2), sum(data(:)));

writer.close();
reader.close();
