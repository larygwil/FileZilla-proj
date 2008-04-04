#! /usr/bin/python

import MySQLdb
import os
import sys
import pysvn
import datetime

import database
db = MySQLdb.connect(db=database.database, user=database.user, passwd=database.password)

typedata = [ {'type': 'text',   'name': 'header', 'extensions': ['.h']},
             {'type': 'text',   'name': 'source', 'extensions': ['.cpp', '.c', '.rc', '.rc2'], 'files': ['version.rc.in']},
             {'type': 'text',   'name': 'other',  'extensions': ['.sh', '.example', '.desktop', '.am', '.m4', '', '.htm', '.html', '.man', '.xrc', '.xml', '.vcproj', '.in', '.kdevelop', '.sln', '.txt', '.def', '.plist', '.nsi'], 'files': ['configure.in', 'autogen.sh']},
             {'type': 'binary', 'name': 'binary', 'extensions': ['.png', '.ico', '.dll', '.xpm', '.icns', '.wav']}
           ]

def update_data(data, fullpath):
  data['count'] += 1
  data['size'] += os.path.getsize(fullpath)
  if data['type'] == 'text':
    data['lines'] += len(open(fullpath).readlines())

def calc(source_directory, revision, timestamp):
  for data in typedata:
    data['count'] = 0;
    data['lines'] = 0;
    data['size'] = 0;
    if not data.has_key('files'):
      data['files'] = []

  for root, dirs, files in os.walk(source_directory):
    # Don't visit .svn dirs
    if '.svn' in dirs:
      dirs.remove('.svn')

    for file in files:
      type = os.path.splitext(file)[1];

      # Skip locales
      if type == '.po':
        continue
      if type == '.pot':
        continue

      fullpath = os.path.join(root, file)
      for data in typedata:
        if type in data['extensions'] or file in data['files']:
          update_data(data, fullpath)
          break
      else:
        print "Unhandled", fullpath
        sys.exit(1)
  
  #for data in typedata:
  #  if data['type'] == 'text':
  #    print 'Files: %d, size: %d, lines: %d, matches %s%s' % (data['count'], data['size'], data['lines'], data['extensions'], data['files'])
  #  else:
  #    print 'Files: %d, size: %d, matches %s%s' % (data['count'], data['size'], data['extensions'], data['files'])

  query='INSERT INTO metrics (revision'
  for data in typedata:
    name = data['name']
    if data['type'] == 'text':
      query += ", loc_" + name
    query += ", size_" + name
    query += ", count_" + name

  query += ') VALUES (%d' % revision

  for data in typedata:
    if data['type'] == 'text':
      query += ', %d' % data['lines']
    query += ', %d' % data['size']
    query += ', %d' % data['count']

  query += ')'

  d = datetime.datetime.fromtimestamp(timestamp)
  datestr = d.strftime('%Y%m%d%H%M%S')

  cursor = db.cursor()
  cursor.execute("DELETE FROM metrics WHERE revision=%d" % revision)
  cursor.execute("DELETE FROM revisions WHERE revision=%d" % revision)
  cursor.execute("INSERT INTO revisions VALUES (%d, %s)" % (revision, datestr))
  cursor.execute(query)
  cursor.close()
  db.commit()
