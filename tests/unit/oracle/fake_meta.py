import web
import json
        
urls = (
    '/(.*)', 'hello'
)
app = web.application(urls, globals())

class hello:        
    def GET(self, name):
        if name == "self" or name == "user/login":
            return json.dumps({'success': True, "_id": "id", 'token':
                'token'})
        return json.dumps({'success': True})
    def POST(self, name):
        return self.GET(name)

if __name__ == "__main__":
    app.run()
