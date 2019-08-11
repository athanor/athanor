#!/usr/bin/env python3
import urllib
import logging
from http.server import HTTPServer, BaseHTTPRequestHandler, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn
import threading
import json
import subprocess
USE_HTTPS = False

def cleanError(error,prefix):
    return error[error.find(prefix) + len(prefix):].strip()
def essenceToJson(essence,isParam):
    tempFileName = "../temp_" + str(threading.get_ident())
    tempFileName += ".essence" if not isParam else ".param"
    with open(tempFileName,"w") as tempFile:
        print(essence,file=tempFile)
    conjureTypeCheckCommand = ["../conjure", "type-check", tempFileName]
    conjureJsonCommand = ["../conjure", "pretty", tempFileName, "--output-format", "json"]
    proc = subprocess.run(conjureTypeCheckCommand,capture_output=True,encoding="utf-8")
    if proc.returncode != 0:
        return { "status":"error", "data":cleanError(proc.stderr,tempFileName + ":")}
    proc = subprocess.run(conjureJsonCommand,capture_output=True,encoding="utf-8")
    if proc.returncode != 0:
        return { "status":"error", "data":proc.stderr }
    
    return {"status":"success","data":proc.stdout}


class Handler(SimpleHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers['Content-Length'])
        postData = json.loads(self.rfile.read(length).decode('utf-8'))
        print("post data = " + str(postData))
        logging.info("POST request,\nPath: %s\nHeaders:\n%s\n\nBody:\n%s\n",
                str(self.path), str(self.headers), postData)
        response = {}
        response["spec"] = essenceToJson(postData["spec"],False)
        response["param"] = essenceToJson(postData["param"], True)
        jsonString = json.dumps(response)
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(bytes(jsonString, "UTF-8"))
        print(jsonString)



class ThreadingSimpleServer(ThreadingMixIn, HTTPServer):
    pass

def run():
    server = ThreadingSimpleServer(('localhost', 8000), Handler)
    if USE_HTTPS:
        import ssl
        server.socket = ssl.wrap_socket(server.socket, keyfile='./key.pem', certfile='./cert.pem', server_side=True)
    server.serve_forever()


if __name__ == '__main__':
    run()
