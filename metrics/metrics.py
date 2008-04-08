#! /usr/bin/python

import MySQLdb
import os
import sys
import pysvn
import datetime

import database
db = MySQLdb.connect(db=database.database, user=database.user, passwd=database.password)

typedata = None;
def init_types(repository):
  global typedata
  typedata = []

  cursor = db.cursor()

  cursor.execute('SELECT `repo_type_assoc`.`type`, `filetypes`.`text` FROM `repo_type_assoc` INNER JOIN `filetypes` WHERE `repo_type_assoc`.`type` = `filetypes`.`id` AND `repo_type_assoc`.`repository` = %d' % repository)

  for index, text, in cursor.fetchall():
    data = {'index': index, 'text': text}

    cursor2 = db.cursor()
    cursor2.execute('SELECT `extension` FROM `extensions` WHERE `filetype` = %d' % index)
    data['extensions'] = [extension for extension, in cursor2.fetchall()]
    
    cursor2.execute('SELECT `filename` FROM `filenames` WHERE `filetype` = %d' % index)
    data['files'] = [filename for filename, in cursor2.fetchall()]

    typedata.append(data)
    cursor2.close()

  cursor.close()

#typedata = [ {'type': 'text',   'name': 'header', 'extensions': ['.h']},
#             {'type': 'text',   'name': 'source', 'extensions': ['.cpp', '.c', '.rc', '.rc2'], 'files': ['version.rc.in']},
#             {'type': 'text',   'name': 'other',  'extensions': ['.ini', '.sh', '.example', '.desktop', '.am', '.m4', '', '.htm', '.html', '.man', '.xrc', '.xml', '.vcproj', '.in', '.kdevelop', '.sln', '.txt', '.def', '.plist', '.nsi'], 'files': ['configure.in', 'autogen.sh']},
#             {'type': 'binary', 'name': 'binary', 'extensions': ['.png', '.ico', '.dll', '.xpm', '.icns', '.wav']}
#           ]

def update_data(data, fullpath):
  data['count'] += 1
  data['size'] += os.path.getsize(fullpath)
  if data['text'] == 1:
    data['lines'] += len(open(fullpath).readlines())

def calc(repository, source_directory, revision, timestamp):
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
      ext = os.path.splitext(file)[1];

      # Skip locales
      if ext == '.po':
        continue
      if ext == '.pot':
        continue

      fullpath = os.path.join(root, file)
      for data in typedata:
        if ext in data['extensions'] or file in data['files']:
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

  cursor = db.cursor()
  
  d = datetime.datetime.fromtimestamp(timestamp)
  datestr = d.strftime('%Y%m%d%H%M%S')

  cursor.execute("DELETE FROM revisions WHERE revision=%d" % revision) # same revision deleted from `metrics` due to foreign key constraint
  cursor.execute("INSERT INTO revisions VALUES (%d, %d, %s)" % (repository, revision, datestr))

  for data in typedata:
   query='INSERT INTO `metrics` VALUES (%d, %d, %d, %d, %d, ' % (repository, data['index'], revision, data['count'], data['size'])
   if data['text'] == 1:
     query += str(data['lines'])
   else:
     query += 'NULL'
   query += ')'
   cursor.execute(query)

  cursor.close()
  db.commit()
