"""
this file is for testing the index.html file, with server side API simulation
"""

from flask import Flask, render_template, jsonify, request, session, redirect, url_for
import logging, json
from flask_socketio import SocketIO, emit, join_room, leave_room
app = Flask(__name__)
socketio = SocketIO()
socketio.init_app(app=app)

@app.route("/")
def index():
    return app.send_static_file('index.html')

if __name__=='__main__':
    socketio.run(app, host='0.0.0.0', port=80)
