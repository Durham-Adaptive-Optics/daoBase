classdef daoShm
    properties (Access = public)
        ImagePtr % Pointer to the IMAGE structure returned by daoShmInit
    end
    
    methods
        function obj = daoShm(name, data)
            % Constructor
            %   obj = daoShm(name, [data]) initializes a daoShm object and the
            %   associated IMAGE structure.
            %
            %   Parameters:
            %   name - Name of the IMAGE structure.
            %   data (optional) - Data to initialize the IMAGE structure.

            if nargin < 1 || nargin > 2
                error('Invalid number of input arguments.');
            end

            % Set default values for optional parameter
            if nargin < 2
                data = [];
            end

            % Call the init method to initialize the IMAGE structure
            obj = obj.init(name, data);
        end
        
        function obj = init(obj, name, data)
            % INIT Initialize an IMAGE structure.
            %   obj = obj.init(name, [data]) initializes an IMAGE structure using
            %   the specified parameters.
            %
            %   Parameters:
            %   name - Name  IMAGE structure.
            %   data (optional) - Data to initialize the IMAGE structure.

            if nargin < 2 || nargin > 3
                error('Invalid number of input arguments.');
            end

            % Set default values for optional parameter
            if nargin < 3
                % Call the MEX function to initialize the IMAGE structure
                obj.ImagePtr = daomex('shm_init', name);
            else
                obj.ImagePtr = daomex('shm_init', name, data);
            end

        end
        
        function data = get_data(obj, check, semNb)
            % GET_DATA Get data from the IMAGE structure.
            %   data = obj.get_data([check, semNb]) retrieves data from
            %   the initialized IMAGE structure.
            %
            %   Parameters (optional):
            %   check - Whether to wait for semaphore (logical).
            %   semNb - Semaphore number (integer).

            if nargin < 2
                check = false; % Default: false
            end
            if nargin < 3
                semNb = 1; % Default: 1
            end

            % Call the MEX function to get data from the IMAGE structure
            data = daomex('get_data', obj.ImagePtr, check, semNb);
        end
        
        function set_data(obj, data)
            % SET_DATA Set data in the IMAGE structure.
            %   obj.set_data(data) sets data in the initialized IMAGE structure.
            %
            %   Parameters:
            %   data - Data to set in the IMAGE structure.
            
            if nargin < 2
                error('Missing data argument.');
            end

            % Call the MEX function to set data in the IMAGE structure
            daomex('set_data', obj.ImagePtr, data);
        end
        
        function counter = get_counter(obj)
            % GET_COUNTER Get the counter value from the IMAGE structure.
            %   counter = obj.get_counter() retrieves the counter value from
            %   the initialized IMAGE structure.

            % Call the MEX function to get the counter value from the IMAGE structure
            counter = daomex('get_counter', obj.ImagePtr);
        end
    end
end
