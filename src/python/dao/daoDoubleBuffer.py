class DoubleBuffer:
    """A class for double buffering data."""

    def __init__(self):
        """Initialize the buffers and current buffer index."""
        self.buffer1 = None
        self.buffer2 = None
        self.current_buffer = 1
        self.new_data = False

    def write(self, data):
        """Write data to the non-current buffer and switch the current buffer."""
        if self.current_buffer == 1:
            self.buffer1 = data
            self.current_buffer = 2
        else:
            self.buffer2 = data
            self.current_buffer = 1
        self.new_data = True

    def read(self):
        """Return the buffer that was not written to in the last write operation."""
        self.new_data = False
        if self.current_buffer == 1:
            return self.buffer2
        else:
            return self.buffer1

    def data_available(self):
        """Return whether new data has been written since the last read operation."""
        return self.new_data
