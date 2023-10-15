from random import randint

from locust import HttpUser, task

class HelloWorldUser(HttpUser):

    @task
    def add(self):
        a = randint(-100, 100)
        b = randint(-100, 100)
        self.client.get("/ping")
        self.client.get(f"/add?a={a}&b={b}")
        self.client.get(f"/sub?a={a}&b={b}")
        self.client.get(f"/mult?a={a}&b={b}")
        self.client.get(f"/div?a={a}&b={b}")
