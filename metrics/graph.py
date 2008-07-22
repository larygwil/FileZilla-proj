#! /usr/bin/python

import MySQLdb
import os
import sys
import time
import rrdtool

imagedir = '/var/www/code/'

import database
db = MySQLdb.connect(db=database.database, user=database.user, passwd=database.password)

colors = [ '#009933', '#003399', '#993300', '#bbbb00' ]

def get_lines(graph):

  cursor = db.cursor()
  cursor.execute('SELECT `id`, `stack`, `area`, `name` FROM `lines` WHERE `graph` = %d' % graph)
  
  return cursor.fetchall()

def get_columns(line):

  cursor = db.cursor()
  cursor.execute('SELECT `type`, `column`, `operator` FROM `line_columns` WHERE `line` = %d ORDER BY `id` ASC' % line)

  return cursor.fetchall()

def get_type_name(type):

  cursor = db.cursor()
  cursor.execute('SELECT `name` FROM `filetypes` WHERE `id` = %d' % type)

  return cursor.fetchall()[0]

def create_graph(graph):

  repository = graph[1]

  cursor = db.cursor()
  cursor.execute('SELECT `date` FROM `revisions` WHERE `repository` = %d' % repository)
  dates = [ date for (date, ) in cursor.fetchall() ]

  metrics = {}

  lines = get_lines(graph[0])

  if len(lines) == 0:
    print 'No line definitions found for graph %d' % graph[0]
    return

  for i, line in enumerate(lines):
    line_id = line[0]

    columns = get_columns(line_id)

    # Calculation stack
    tmp = {}
    stackdepth = 0

    for j, op in enumerate(columns):

      if op[2] == '/':

        for k, v in enumerate(tmp[stackdepth - 2]):
	  if tmp[stackdepth - 1][k] == 0:
	    tmp[stackdepth-2][k] = 0
	  else:
            tmp[stackdepth - 2][k] = v / tmp[stackdepth - 1][k]
	stackdepth = stackdepth - 1

      else:
        line_type = op[0]
        column = op[1]
        if column == 0:
          columnname = 'count'
        elif column == 1:
          columnname = 'size'
        else:
          columnname = 'lines'

        cursor.execute('SELECT `%s` FROM `metrics` WHERE `repository` = %d AND `filetype` = %d' % (columnname, repository, line_type))

        tmp[stackdepth] = [ metric for (metric, ) in cursor.fetchall() ]
        stackdepth = stackdepth + 1

	if line[3] == None:
	  line = line[:-1] + get_type_name(line_type)
	  lines = lines[:i] + (line,) + lines[i+1:]

    if stackdepth != 1:
      print 'Invalid line definition, nonzero stackdepth for graph %d' % graph[0]
      return

    metrics[i] = tmp[0]

  initialised = False
  prev=0
  starttime=0
  for dateindex, date in enumerate(dates):
  
    timestamp = int(time.mktime(time.strptime(str(date), '%Y-%m-%d %H:%M:%S')))
  
    if not initialised:
      args = [ '/home/metrics/tmp/tmp.rrd', 
               '--start', str(timestamp - 1)
             ]

      for i in range(0, len(metrics)):
        args.append('DS:source' + str(i) + ':GAUGE:9999999:U:U')

      args.append('RRA:AVERAGE:0.1:22:2200200')
      rrdtool.create(*args)
      initialised = True
      starttime = timestamp
  
    if prev != timestamp:

      s = str(timestamp)
      for i in range(0, len(metrics)):
        if metrics[i][dateindex] == None:
	  s += ':U'
	else:
          s += ':%d' % metrics[i][dateindex]
      rrdtool.update('../tmp/tmp.rrd', s)
      prev = timestamp
 
  file = imagedir + 'test%d.png' % graph[0]
  args = [file,
          '--imgformat', 'PNG',
          '--width', '520',
          '--height', '250',
          '--start', "%d" % starttime,
#         '--end', "-1",
          '--vertical-label', graph[3],
          '--title', graph[2],
          '--lower-limit', '0']

  for i, line in enumerate(lines):
    name = line[3]

    #flags = line[2:]
    args.append('DEF:source' + str(i) + '=../tmp/tmp.rrd:source' + str(i) + ':AVERAGE')

    if line[2] == 1:
      graph_def = 'AREA:'
    else:
      graph_def = 'LINE:'
    graph_def += 'source' + str(i) + colors[i] + ':' + name

    if line[1] == 1:
      graph_def += ':STACK'
    args.append(graph_def)

  rrdtool.graph(*args)

cursor = db.cursor()
cursor.execute('SELECT `id`, `repository`, `title`, `y_axis` FROM `graphs`')

graphs = cursor.fetchall()

for graph in graphs:

  create_graph(graph)

db.close()

