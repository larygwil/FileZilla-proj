#! /usr/bin/python

import pysvn
import metrics
import database
import MySQLdb

svn_directory="/home/metrics/metrics/svn/"

db = MySQLdb.connect(db=database.database, user=database.user, passwd=database.password)

# Function to go through all revisions
def traverse(repository, subdir):
  client = pysvn.Client()

  source_directory = svn_directory + subdir 

  current_revision = client.info(source_directory).revision.number
  logs = client.log(source_directory, revision_start=pysvn.Revision(pysvn.opt_revision_kind.number, current_revision), revision_end=pysvn.Revision(pysvn.opt_revision_kind.head))

  for log in logs:
    revision = log['revision'].number
    print "Getting metrics for revision", log['revision'].number
    client.update(source_directory, revision=pysvn.Revision(pysvn.opt_revision_kind.number, revision))
    metrics.calc(repository, source_directory, revision, log['date'])

cursor = db.cursor()
cursor.execute("SELECT `id`, `svndir` FROM `repository`")
for row in cursor.fetchall():
  metrics.init_types(row[0])
  traverse(*row)

cursor.close()
db.close()
