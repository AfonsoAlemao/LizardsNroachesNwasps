import zmq
import lizards_pb2
import random
import time

def main(server_address, port):
    context = zmq.Context()
    requester = context.socket(zmq.REQ)
    requester.connect(f"tcp://{server_address}:{port}")

    # Generate a unique id based on the address and port
    id_string = f"{server_address}:{port}"
    id_int = hash(id_string) & 0x7FFFFFFF # hash function for id

    # Ask user for the number of roaches
    try:
        nchars = int(input("Enter the number of roaches (1-10): "))
        if nchars < 1 or nchars > 10:
            raise ValueError
    except ValueError:
        print("Invalid input. Please enter a number between 1 and 10.")
        return

    # Create a RemoteChar message
    m = lizards_pb2.RemoteChar()
    m.msg_type = 0
    m.id = id_int
    m.nchars = nchars

    for _ in range(m.nchars):
        m.ch += str(random.randint(1, 5))
        m.direction.append(random.choice(list(lizards_pb2.Direction.values())))

    # Send connection message
    requester.send(m.SerializeToString())
    reply = requester.recv()
    ok_message = lizards_pb2.ok_message()
    ok_message.ParseFromString(reply)

    if ok_message.msg_ok == 0:
        print("Connection failed or request not fulfilled")
        return

    while True:
        time.sleep(random.uniform(0.1, 1))
        m.msg_type = 1

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
        print("Usage: python client.py <server-address> <port>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
