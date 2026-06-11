import os
import secrets
from functools import wraps
from flask import request, jsonify

# Pobieranie danych logowania ze zmiennych środowiskowych
API_USERNAME = os.environ.get("API_USERNAME", "studentDebil")
API_PASSWORD = os.environ.get("API_PASSWORD", "student")

def _is_valid_basic_auth(auth):
    if auth is None:
        return False
    
    username = auth.username or ""
    password = auth.password or ""
    
    # Bezpieczne porównanie odporne na ataki typu 'timing attack'
    username_ok = secrets.compare_digest(username, API_USERNAME)
    password_ok = secrets.compare_digest(password, API_PASSWORD)
    
    return username_ok and password_ok

def unauthorized_response():
    response = jsonify({
        "error": "unauthorized",
        "message": "Missing or invalid credentials"
    })
    # Zgodnie z RFC 7617 zwracamy kod 401 oraz nagłówek WWW-Authenticate
    return response, 401, { "WWW-Authenticate": 'Basic realm="RSP API"' }

def auth_required(view_function):
    @wraps(view_function)
    def wrapper(*args, **kwargs):
        if not _is_valid_basic_auth(request.authorization):
            return unauthorized_response()
        return view_function(*args, **kwargs)
    return wrapper