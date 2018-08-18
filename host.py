"""
this file is for testing the index.html file, with server side API simulation
"""

PWM_FREQUENCY = 20000
PWM_RESOLUTION = 10
MAX_PWM_DUTY = 2**PWM_RESOLUTION - 1

from flask import Flask, render_template, jsonify, request, session, redirect, url_for
import logging, json, time
from flask_socketio import SocketIO, emit, join_room, leave_room
app = Flask(__name__)
socketio = SocketIO()
socketio.init_app(app=app)

@app.route("/")
def index():
    return app.send_static_file('index.html')

mode = "PWM"
pwm = [0, 0]
def nowState():
    global mode
    global pwm
    return jsonify({
        'mode': mode,
        'heap': '666',
        'ssid': 'CDMAwtf',
        'millis': int(time.time() * 1000),
        'pwm0': pwm[0],
        'pwm1': pwm[1]
    })

@app.route("/sync", methods=['GET'])
def sync():
    return nowState()

@app.route("/setMode", methods=['POST'])
def setMode():
    global mode
    if 'mode' in request.form:
        print('set mode to: ' + request.form['mode'])
        mode = request.form['mode']
    return nowState()

@app.route("/setPWM", methods=['POST'])
def setPWM():
    global pwm
    if 'pwm0' in request.form:
        print('set pwm0 to: ' + request.form['pwm0'])
        pwm0 = int(request.form['pwm0'])
        if pwm0 > MAX_PWM_DUTY: pwm0 = MAX_PWM_DUTY
        if pwm0 < -MAX_PWM_DUTY: pwm0 = -MAX_PWM_DUTY
        pwm[0] = pwm0
    if 'pwm1' in request.form:
        print('set pwm1 to: ' + request.form['pwm1'])
        pwm1 = int(request.form['pwm1'])
        if pwm1 > MAX_PWM_DUTY: pwm1 = MAX_PWM_DUTY
        if pwm1 < -MAX_PWM_DUTY: pwm1 = -MAX_PWM_DUTY
        pwm[1] = pwm1
    return nowState()

if __name__=='__main__':
    socketio.run(app, host='0.0.0.0', port=80)
