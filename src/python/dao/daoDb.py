import redis
import numpy  as np
import json

# A class that interacts with Redis database and fires an event when there's a change in the database
class RedisEventHandler:
    def __init__(self, redis_client, key):
        # Store the Redis client instance
        self.redis_client = redis_client

        # Create a pubsub instance
        self.pubsub = self.redis_client.pubsub()

        # Subscribe to all key-related events (__keyevent@0__:*) in Redis database
        self.pubsub.psubscribe(key)

    def listen(self):
        # Listen for messages on the pubsub channel
        for message in self.pubsub.listen():
            # Check if the message is a pattern-based message
            #if message["type"] == "pmessage":
            self.on_event(message)
    
    def on_event(self, message):
        print("Event fired:", message)

def store_numpy_array_in_redis(r, key, np_array, chunk_size=1024*1024*50, uid=0):
    # Get the shape and dtype of the array
    shape = np_array.shape
    dtype = np_array.dtype

    # Split the array into chunks and store each chunk in Redis
    i = 0
    for chunk in np.array_split(np_array, np.ceil(np_array.nbytes / chunk_size)):
        chunk_key = f'{key}:data:{i}'
        r.set(chunk_key, chunk.tobytes())
        i += 1

    # Store the shape and dtype in Redis as a JSON string
    meta = {'shape': shape, 'dtype': str(dtype), 'uid': uid}
    r.set(f'{key}:meta', json.dumps(meta))
    # publish the meta also as a message can be used by a pub/sub is needed
    r.publish(f'{key}:message', json.dumps(meta))

    return meta

def get_numpy_array_from_redis(r, key):
    # Get the shape and dtype of the array from Redis
    meta = json.loads(r.get(f'{key}:meta'))
    shape = meta['shape']
    dtype = np.dtype(meta['dtype'])
    uid = meta['uid']

    # Get the chunks of data from Redis and concatenate them into a NumPy array
    chunks = []
    for chunk_key in sorted(r.scan_iter(f'{key}:data:*')):
        chunk = r.get(chunk_key)
        chunks.append(chunk)
    data = b''.join(chunks)
    np_array = np.frombuffer(data, dtype=dtype).reshape(shape)

    return np_array, meta



if __name__=="__main__":
    # Connect to the Redis database using the default host and port
    redis_client = redis.Redis(host="localhost", port=6379)

    # Create an instance of RedisEventHandler
    redis_event_handler = RedisEventHandler(redis_client, 'key1')

    # Start listening for changes in the Redis database
    redis_event_handler.listen()
