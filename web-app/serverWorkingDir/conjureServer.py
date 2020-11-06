#!/usr/bin/env python3
import urllib
import time
import logging
from http.server import HTTPServer, BaseHTTPRequestHandler, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn
import threading
import json
import subprocess
import sys
USE_HTTPS = False

pathWhiteList = {"/",
    "/athanor.js",
    "/athanor.wasm",
    "/athanorWorker.js",
    "/essenceInput.js",
    "/index.html"}
conjurePath = "../conjure"

def findEndOf(text,substr):
    i = text.find(substr)
    return i + len(substr) if i != -1 else i
def removeFileNames(text):
    """this is just a prettyness thing, not a security thing"""
    startIndex = findEndOf(text,".essence:")
    if startIndex != -1:
        return "In specification:" + text[startIndex:]
    startIndex = findEndOf(text,".param:")
    if startIndex != -1:
        return "In parameter:" + text[startIndex:]
    return text




def essenceToJson(essence,isParam):
    tempFileName = "../temp_" +  str(time.time()) + "_" + str(threading.get_ident()) 
    tempFileName += ".essence" if not isParam else ".param"
    with open(tempFileName,"w") as tempFile:
        print(essence,file=tempFile)
    conjureTypeCheckCommand = [conjurePath, "type-check", tempFileName]
    conjureJsonCommand = [conjurePath, "pretty", tempFileName, "--output-format", "json"]
    proc = subprocess.run(conjureTypeCheckCommand,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        return { "status":"error", "data":removeFileNames(proc.stdout.decode('utf-8'))}
    proc = subprocess.run(conjureJsonCommand,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        return { "status":"error", "data":proc.stdout.decode('udf-8') }
    
    return {"status":"success","data":proc.stdout.decode('utf-8')}


class Handler(SimpleHTTPRequestHandler):
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        #hack so that if the path is not in the white list, make the path garbidge so it fails to return anything
        if self.path not in pathWhiteList:
            self.path = "unknown"
        return super().do_GET()
    def do_POST(self):
        length = int(self.headers['Content-Length'])
        postData = json.loads(self.rfile.read(length).decode('utf-8'))
        response = {}
        response["spec"] = essenceToJson(postData["spec"],False)
        response["param"] = essenceToJson(postData["param"], True)
        jsonString = json.dumps(response)
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(bytes(jsonString, "UTF-8"))



class ThreadingSimpleServer(ThreadingMixIn, HTTPServer):
    pass

def run():
    server = ThreadingSimpleServer(('127.0.0.1', 8000), Handler)
    if USE_HTTPS:
        import ssl
        server.socket = ssl.wrap_socket(server.socket, keyfile='./key.pem', certfile='./cert.pem', server_side=True)
    server.serve_forever()


if __name__ == '__main__':
    if len(sys.argv) == 2:
        conjurePath = sys.argv[1]
    run()
