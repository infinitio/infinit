# -*- encoding: utf-8 -*-

import pymongo

try:
    from pymongo.objectid import ObjectId
except ImportError:
    from bson.objectid import ObjectId

try:
    from pymongo.binary import Binary
except ImportError:
    from bson.binary import Binary

# functions
def byId(collection, _id):
    """
    Get an object from collection `collection' with its id `_id'
    """
    return collection.find_one({'_id': ObjectId(_id)})
