import zmq
import lizards_pb2  # Import the generated protobuf module
import random
import time

def main(server_address, port):
    context = zmq.Context()
    requester = context.socket(zmq.REQ)
    requester.connect(f"tcp://{server_address}:{port}")

    # Generate a unique ID based on the address and port
    id_string = f"{server_address}:{port}"
    id_int = hash(id_string) & 0x7FFFFFFF  # Simple hash function for ID, ensuring it fits within int32 range

    nwasps = int(input("How many wasps (1 to 10)? "))
    while nwasps < 1 or nwasps > 10:
        print("Invalid input. Please enter a number in the range 1 to 10.")
        nwasps = int(input("How many wasps (1 to 10)? "))

    # Create a RemoteChar message for connection
    m = lizards_pb2.RemoteChar()
    m.msg_type = 6  # Connection message for wasps
    m.id = id_int
    m.nchars = nwasps

    for _ in range(m.nchars):
        m.ch += '#'  # Assign '#' to each wasp
        m.direction.append(random.choice(list(lizards_pb2.Direction.values())))

    # Send connection message
    requester.send(m.SerializeToString())
    reply = requester.recv()
    ok_message = lizards_pb2.ok_message()
    ok_message.ParseFromString(reply)

    if ok_message.msg_ok == 0 or ok_message.msg_ok == ord('?'):
        print("Connection failed or request not fulfilled")
        return

    while True:
        time.sleep(random.uniform(0, 1))  # Random delay up to 1 second
        m.msg_type = 7  # Movement message for wasps

        # Update directions randomly
        m.direction[:] = [random.choice(list(lizards_pb2.Direction.values())) for _ in range(m.nchars)]

        requester.send(m.SerializeToString())
        reply = requester.recv()
        ok_message.ParseFromString(reply)

        if ok_message.msg_ok == 0:
            print("Movement request not fulfilled")
            break

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python wasps_client.py <server-address> <port>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
