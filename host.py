"""
this file is for testing the index.html file, with server side API simulation
"""

from flask import Flask, render_template, jsonify, request, session, redirect, url_for
import logging, json
app = Flask(__name__)

@app.route("/")
def index():
    return app.send_static_file('index.html')

if __name__=='__main__':
    app.run(host='0.0.0.0', port=80)
