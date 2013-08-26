#!/usr/bin/env python2.7
# -*- encoding: utf-8 -*-

from __future__ import print_function

from twisted.internet import protocol, reactor, task
from twisted.protocols import basic
from twisted.python import log

import os
import json
import time
import pythia
import pprint
import clients

response_matrix = {
    # Success code
    200 : "OK",
    202 : "accepted",

    # Client Error code
    400 : "bad_request",
    403 : "forbidden",
    404 : "not_found",
    418 : "im_a_teapot",

    #  Server error
    500 : "internal_server",
    666 : "unknown_error",
}

class InvalidID(Exception):

    def __init__(self, id):
        self.id = id
        pass

    def __str__(self):
        return "{} invalid id".format(self.id)

class Trophonius(basic.LineReceiver):

    states = ('HELLO',
              'PING')

    delimiter = "\n"
    def __init__(self, factory):
        self.factory = factory
        self.id = None
        self.token = None
        self.device_id = None
        self.state = 'HELLO'
        self.meta_client = None
        self._alive_service = None
        self._ping_service = None
        self.reason = None

    def __deinit(self):
        # It's actually important to remove references of this instance
        # in the clients dictionary, else the memory won't be free
        if self.device_id in self.factory.clients:
            self.factory.clients.pop(self.device_id)
        self.factory = None
        self.id = None
        self.token = None
        self.device_id = None
        self.state = None
        self.meta_client = None
        self._alive_service = None
        self._ping_service = None
        self.reason = None

    def __str__(self):
        if hasattr(self, "id"):
            return "<{}({})>".format(self.__class__.__name__,
                                                 "id={}".format(self.id))
        return "<{}()>".format(self.__class__.__name__)

    def connectionMade(self):
        print(self.connectionMade, self.transport.getPeer())
        self._ping_service = task.LoopingCall(self.sendLine,
                json.dumps({"notification_type": 208}))
        self._ping_service.start(self.factory.application.timeout / 2)
        timeout = self.factory.application.timeout
        self._alive_service = reactor.callLater(timeout, self._loseConnection)

    def _loseConnection(self):
        self.reason = "noPing"
        self.transport.loseConnection()

    def connectionLost(self, reason):
        self.reason = self.reason or reason.getErrorMessage()

        print(self.connectionLost, self.transport.getPeer(), self.reason)

        try:
            if self.id is None:
                return # self.__deinit() still called !

            if self._alive_service is not None and self._alive_service.active():
                self._alive_service.cancel()

            if self._ping_service is not None:
                self._ping_service.stop()

            log.msg("Disconnect user %s" % self.id)

            if self.meta_client is not None:
                self.meta_client.post('/user/disconnect', {
                    'user_id': self.id,
                    'device_id': self.device_id,
                })
        finally:
            self.__deinit()

    def _send_res(self, res, msg=""):
        if isinstance(res, dict):
            self.sendLine(json.dumps(res))
        elif isinstance(res, int):
            s = {}
            s["notification_type"] = -666
            s["response_code"] = res
            if msg:
                s["response_details"] = "{}: {}".format(response_matrix[res], msg)
            else:
                s["response_details"] = "{}".format(response_matrix[res])
            message = json.dumps(s)
            log.msg("sending message from", self, "to", self.transport.getPeer(), ":", message)
            self.sendLine("{}".format(message))

    def handle_PING(self, line):
        data = json.loads(line)
        if data["notification_type"] == 208:
            if self._alive_service is not None and \
               self._alive_service.active():
                timeout = self.factory.application.timeout
                self._alive_service.reset(timeout)

    def handle_HELLO(self, line):
        """
        This function handle the first message of the client
        It waits for a json object with:
        {
            "token": <token>,
            "device_id": <device_id>,
            "user_id": <user_id>,
        }
        """
        try:
            req = json.loads(line)
            self.device_id = req["device_id"]

            # Authentication
            client = pythia.Client(session={'token': req['token']},
                    server=self.factory.application.meta_url)
            res = client.get('/self')
            if not res['success']:
                raise Exception("Meta error: %s" % res.get('error', ''))
            self.id = res["_id"]
            self.token = req["token"]

            self.meta_client = pythia.Admin(server=self.factory.application.meta_url)
            res = self.meta_client.post('/user/connect', {
                'user_id': self.id,
                'device_id': self.device_id,
            })
            log.msg("User", self.id, "is now connected with device", self.device_id)

            # Add the current client to the client list
            assert isinstance(self.factory.clients, dict)
            self.factory.clients[self.device_id] = self

            # Enable the notifications for the current client
            self.state = "PING"

            # Send the success to the client
            self._send_res(res = 200)
        except (ValueError, KeyError) as ve:
            log.err("Handled exception {} in state {}: {}".format(
                                        ve.__class__.__name__,
                                        self.state,
                                        ve))
            self._send_res(res=400, msg=ve.message)
            self.transport.loseConnection()

    def lineReceived(self, line):
        hdl = getattr(self, "handle_{}".format(self.state), None)
        if hdl is not None:
            hdl(line)

class MetaTropho(basic.LineReceiver):

    delimiter = "\n"

    def __init__(self, factory):
        self.factory = factory

    def connectionMade(self):
        pass #log.msg("Meta: New connection from", self.transport.getPeer())

    def connectionLost(self, reason):
        pass #log.msg("Meta: Connection lost with", self.transport.getPeer(), reason.getErrorMessage())

    def _send_res(self, res, msg=""):
        if isinstance(res, dict):
            self.sendLine(json.dumps(res))
        elif isinstance(res, int):
            s = {}
            s["response_code"] = res
            if msg:
                s["response_details"] = "{}: {}".format(response_matrix[res], msg)
            else:
                s["response_details"] = "{}".format(response_matrix[res])
            message = json.dumps(s)
            self.sendLine(message)

    def enqueue(self, line, device_ids):
        try:
            for device_id in device_ids:
                if not device_id in self.factory.clients:
                    #log.msg("Device %s not connected" % device_id)
                    continue
                log.msg("Send %s to %s" % (line, device_id))
                self.factory.clients[device_id].sendLine(str(line))
        except KeyError as ke:
            log.err("Handled exception {}: {} unknow id".format(
                ke.__class__.__name__,
                ke
            ))
            raise InvalidID(device_id)

    def make_switch(self, line):
        try:
            js_req = json.loads(line)
            device_ids = js_req["to_devices"]
            assert isinstance(device_ids, list)
            self.enqueue(line, device_ids)
        except ValueError as ve:
            log.err("Handled exception {}: {}".format(
                                        ve.__class__.__name__,
                                        ve))
            self._send_res(res=400, msg=ve.message)
        except KeyError as ke:
            log.err("Handled exception {}: missing key {} in request".format(
                                        ke.__class__.__name__,
                                        ke))
            self._send_res(res=400, msg="missing key {} in req".format(ke.message))
        except InvalidID as ide:
            self._send_res(res=400, msg="{}".format(str(ide)))
        else:
            self._send_res(res=202, msg="message enqueued")

    def lineReceived(self, line):
        self.make_switch(line)

class TrophoFactory(protocol.Factory):
    def __init__(self, application):
        self.application = application
        self.clients = application.clients
        if self.application.runtime_dir is not None:
            self.sock_path = os.path.join(application.runtime_dir, "trophonius.sock")

    def stopFactory(self):
        if self.application.runtime_dir is not None:
            os.remove(self.sock_path)

    def buildProtocol(self, addr):
        return Trophonius(self)

class MetaTrophoFactory(protocol.Factory):
    def __init__(self, application):
        self.application = application
        self.clients = application.clients
        if self.application.runtime_dir is not None:
            self.sock_path = os.path.join(application.runtime_dir, "trophonius.csock")

    def buildProtocol(self, addr):
        return MetaTropho(self)

    def stopFactory(self):
        if self.application.runtime_dir is not None:
            os.remove(self.sock_path)
