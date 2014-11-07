#!/usr/bin/env python3

class MongoExpectation:

  instance = None

  def __init__(self, index_miss = 0, object_miss = 0):
    self.index_miss = index_miss
    self.object_miss = object_miss

  def __enter__(self):
    self.__previous = MongoExpectation.instance
    MongoExpectation.instance = self

  def __exit__(self, *args, **kwargs):
    MongoExpectation.instance = self.__previous

MongoExpectation.instance = MongoExpectation()

class GestapoMongoClient(pymongo.MongoClient):

  def __getattr__(self, name):
    res = super().__getattr__(name)
    if res.__class__ is pymongo.database.Database:
      res.__class__ = GestapoDatabase
    return res

class GestapoDatabase(pymongo.database.Database):

  def __getattr__(self, name):
    res = super().__getattr__(name)
    if res.__class__ is pymongo.collection.Collection and name != '$cmd':
      res.__class__ = GestapoCollection
    return res

class GestapoCollection(pymongo.collection.Collection):

  def find(self, spec = None, fields = None, *args, **kwargs):
    self.__check(spec, fields)
    return super().find(spec = spec, fields = fields, *args, **kwargs)

  def update(self, spec, document, multi = False):
    self.__check(spec)
    return super().update(spec = spec,
                          document = document,
                          multi = multi)

  def __check(self, condition, fields = None):
    explanation = super().find(condition, fields = fields).explain()
    print(explanation['cursor'])
    print('  ', condition)
    print('  ', fields)
    print('  ', explanation)
    if explanation['cursor'] == 'BasicCursor':
      raise Exception('table scan on condition: %s' % condition)
    if explanation.get('scanAndOrder', False):
      raise Exception('scan and order on condition: %s' % condition)
    nplans = len(list(e for e in explanation['allPlans']
                      if e['cursor'] != 'BasicCursor'))
    if nplans > 1:
      raise Exception('%s viable plans for condition: %s' % (nplans, condition))
    ns = explanation['nscanned']
    nso = explanation['nscannedObjects']
    n = explanation['n']
    print(ns, nso, n)
    if ns - nso > MongoExpectation.instance.index_miss:
      raise Exception('too many index scans (%s) for %s object scans on condition: %s' % (ns, nso, condition))
    if nso - n > MongoExpectation.instance.object_miss:
      raise Exception('too many object scans (%s) for %s results on condition: %s' % (nso, n, condition))
