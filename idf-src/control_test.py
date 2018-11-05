# 测试控制程序，使用UDP进行控制，每100ms发送数据

import socket, sys, time

port = 666
host = '192.168.43.6'
family_addr = socket.AF_INET
TEST_NUM = 100

try:
    sock = socket.socket(family_addr, socket.SOCK_DGRAM) 
except socket.error as msg:
    print('Failed to create socket')
    sys.exit()

rtt = []
for i in range(TEST_NUM):
    try:
        # msg = "echo %d" % i
        msg = "s %d %d %d" % (100 * ((i+3) % 21) - 1000, 100 * ((i+11) % 21) - 1000, 1024)
        start = time.time()
        sock.sendto(msg.encode(), (host, port))
        reply, addr = sock.recvfrom(128)
        if not reply: break
        end = time.time()
        rtt.append(end - start)
        print(end - start)
        # print('Reply[' + addr[0] + ':' + str(addr[1]) + '] - ' + str(reply))
    except socket.error as msg:
        # print('Error Code : ' + str(msg[0]) + ' Message: ' + msg[1])
        sys.exit()
    time.sleep(0.1)

print("average is:", sum(rtt) / len(rtt))
print("maximum is:", max(rtt))
print("minimum is:", min(rtt))
