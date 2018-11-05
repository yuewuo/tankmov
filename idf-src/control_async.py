import socket, sys, time, threading

NEED_REPLY = True

speed1_P = 0
speed1_N = 0
speed2_P = 0
speed2_N = 0
host1 = "192.168.43.6"
host2 = "192.168.43.233"
port = 666
maxspeed = 1024
exitbit = False

def speed_sender(num):
    global speed1_P
    global speed1_N
    global speed2_P
    global speed2_N
    global maxspeed
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) 
    except socket.error as msg:
        print('Failed to create socket')
        sys.exit()
    while not exitbit:
        start = time.time()
        if num == 0:
            msg = "s %d %d %d" % (speed1_P, speed1_N, maxspeed)
            sock.sendto(msg.encode(), (host1, port))
        else:
            msg = "s %d %d %d" % (speed2_P, speed2_N, maxspeed)
            sock.sendto(msg.encode(), (host2, port))
        if NEED_REPLY:
            reply, addr = sock.recvfrom(128)
        end = time.time()
        print(end - start)
        time.sleep(0.1)
    print("num %d exit" % num)
    

t1 = threading.Thread(target=speed_sender, args=(0,))
t1.setDaemon(True)
t1.start()

while True:
    st = input()
    stp = st.split(' ')
    if len(stp) == 1 and stp[0] == "exit":
        exitbit = True
        t1.join()
        break
    elif len(stp) == 3 and stp[0] == "sp1":
        speed1_P = int(stp[1])
        speed1_N = int(stp[2])

