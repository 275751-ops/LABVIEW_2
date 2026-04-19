# TODO
import psycopg2

DB_HOST="database"
DB_NAME="abcd_db"
DB_USER="admin"
DB_PASSWORD="admin_pass1234"

def get_connection() :
    return psycopg2.connect(
        host=DB_HOST,
        dbname=DB_NAME,
        user=DB_USER,
        password=DB_PASSWORD
    )

