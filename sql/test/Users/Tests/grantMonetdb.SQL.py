###
# Grant monetdb rights to a user.
# Verify that the user can assume the monetdb role and CREATE new users, GRANT privileges and roles.
###

from MonetDBtesting.sqltest import SQLTestCase

with SQLTestCase() as tc:
    tc.connect(username="monetdb", password="monetdb")
    tc.execute("GRANT sysadmin TO alice;").assertSucceeded()
    tc.connect(username="alice", password="alice")
    tc.execute("""
    SET ROLE sysadmin;
    CREATE USER may WITH PASSWORD 'may' NAME 'May' SCHEMA library;""").assertFailed(err_code='M0M27')
    tc.execute("GRANT ALL ON orders TO april;").assertSucceeded()
    tc.execute("GRANT sysadmin TO april;").assertFailed(err_code='0P000')

# import os, sys
# try:
#     from MonetDBtesting import process
# except ImportError:
#     import process

# def sql_test_client(user, passwd, input):
#     with process.client(lang="sql", user=user, passwd=passwd, communicate=True,
#                         stdin=process.PIPE, stdout=process.PIPE, stderr=process.PIPE,
#                         input=input, port=int(os.getenv("MAPIPORT"))) as c:
#         c.communicate()

# sql_test_client('monetdb', 'monetdb', input="""\
# GRANT sysadmin TO alice;
# """)


# sql_test_client('alice', 'alice', input="""\
# SET ROLE sysadmin;
# CREATE USER may WITH PASSWORD 'may' NAME 'May' SCHEMA library;
# GRANT ALL ON orders TO april;
# GRANT sysadmin TO april;
# """)


