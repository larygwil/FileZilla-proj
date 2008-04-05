#! /usr/bin/python

import pysvn
import metrics

source_directory="/home/metrics/metrics/FileZilla3"

client = pysvn.Client()

current_revision = client.info(source_directory).revision.number
logs = client.log(source_directory, revision_start=pysvn.Revision(pysvn.opt_revision_kind.number, current_revision), revision_end=pysvn.Revision(pysvn.opt_revision_kind.head))

for log in logs:
  revision = log['revision'].number
  print "Getting metrics for revision", log['revision'].number
  client.update(source_directory, revision=pysvn.Revision(pysvn.opt_revision_kind.number, revision))
  metrics.calc(source_directory, revision, log['date'])

