#!/usr/bin/env python

import os
import re
import urllib
import csv
import datetime
from string import Template

prev_tag = 'EMPATHY_0_21_4'
username = 'xclaesse'

template = '''
$name $version is now available for download from:
$download

$md5sums

What is it?
===========
$about

Where can I find out more?
==========================
You can visit the project web site:
$website

What's New?
===========
$news

$footer
'''

def exec_cmd (cmd):
	return os.popen(cmd).read()

class Commit:
	ref = ''
	author = ''
	date = ''
	message = ''
	bug = ''
	summary = ''
	translation = False

	def parse(self):
		if self.message[len(self.message) - 1] == ')':
			p1 = self.message.rfind('(')
			self.author = self.message[p1+1:len(self.message) - 1]
			self.message = self.message[:p1]

		p1 = self.message.find('#')
		p2 = self.message.find(' ', p1)
		if p1 != -1:
			self.bug = self.message[p1+1:p2]

		message = self.message.lower()
		if message.find('translation') != -1 and\
		   message.find('updated') != -1:
			self.translation = True
			exp = '.*pdated(?P<name>.*).ranslation.*'
			lang_re = re.compile(exp, re.S | re.M)
	                match = lang_re.match(self.message)
			if match:
				lang = match.group('name').strip()				
				self.summary = "Updated " + lang + " Translation"
			else:
				self.summary = self.message
			self.summary += ' (' + self.author + ').'

		return self.bug

class Project:
	package_name = ''
	package_version = ''
	package_module = ''
	package_dl_url = ''
	description = ''
	url = ''
	md5sums = ''
	news = ''
	translations = ''
	new_tag = ''
	notes = ''
	commits = []

	def __init__(self):
		self.get_package_info()
		self.get_new_tag()
		self.get_bugzilla_info()
		self.get_md5sums()
		self.get_news()
		self.get_commits()
		self.get_notes()

	def make_tag(self):
		url1 = exec_cmd('git-config svn-remote.svn.url').strip()
		url2 = url1[:url1.rfind('/')] + '/tags/' + self.new_tag

		exec_cmd('svn copy %s %s -m "Tagged for release %s."' % (url1, url2, self.package_version))
		exec_cmd('git-tag -m "Tagged for release %s." %s' % ( self.package_version, self.new_tag))

	def make_news(self):
		bugs = ''
		translations = ''
		others = ''
		for co in self.commits:
			if co.summary == '':
				others += '- ' + co.message + '\n'
			elif co.translation == False:
				bugs += '- ' + co.summary + '\n'
			else :
				translations += '- ' + co.summary + '\n'
				
		news = 'NEW in '+ self.package_version + '\n==============\n' 
		news += others + '\n' + bugs + '\nTranslations:\n' + translations + '\n'

		f = open ('/tmp/NEWS', 'w')
		s = f.write(news)
		f.close()

		exec_cmd('cat NEWS >> /tmp/NEWS')
		exec_cmd('mv /tmp/NEWS .')

	def upload_tarball(self):
		# This is the tarball we are going to upload
		tarball = '%s-%s.tar.gz' % (package_name.lower(), package_version)
                
		cmd = 'scp %s %s@%s:' % (tarball, username, upload_server)
		exec_cmd(cmd)
                
		cmd = 'ssh %s@%s install-module %s' % (username, upload_server, tarball)
		exec_cmd(cmd)

	def get_new_tag(self):
		self.new_tag = self.package_name.upper() + '_' +\
			       self.package_version.replace('.', '_')

	def get_notes(self):
		name = self.package_name
		version = self.package_version
		download = self.package_dl_url
		md5sums = self.md5sums
		about = self.description
		website = self.url
		news = self.news
		footer = '%s\n%s team' % (datetime.date.today().strftime('%d %B %Y'),\
					  self.package_name)

		t = Template(template)
		self.notes = text = t.substitute(locals())

	def get_news(self):
		f = open ('NEWS', 'r')
		s = f.read()
		f.close()
		start = s.find ('NEW in '+ self.package_version)
		if start != -1:
			start = s.find ('\n', start) + 1
			start = s.find ('\n', start) + 1
	        	end = s.find ('NEW in', start) - 1
		        self.news = s[start:end]

	def get_md5sums(self):
		cmd = 'md5sum %s-%s.tar.gz' % (self.package_name.lower(), self.package_version)
		self.md5sums += exec_cmd(cmd)

		cmd = 'md5sum %s-%s.tar.bz2' % (self.package_name.lower(), self.package_version)
		self.md5sums += exec_cmd(cmd)

	def get_package_info(self):
		f = open('config.h', 'r')
		s = f.read()
		f.close()

		key = {}
		key['package'] = '#define PACKAGE_NAME "'
		key['version'] = '#define PACKAGE_VERSION "'
		key['bugreport'] = '#define PACKAGE_BUGREPORT "'

		for line in s.splitlines(1):
			if line.startswith(key['package']):
				p1 = len(key['package'])
				p2 = line.rfind('"')
				self.package_name = line[p1:p2] 		
			elif line.startswith(key['version']):
				p1 = len(key['version'])
				p2 = line.rfind('"')
				self.package_version = line[p1:p2] 		
			elif line.startswith(key['bugreport']):
				p2 = line.rfind('"')
				p1 = line.rfind('=') + 1
				self.package_module = line[p1:p2] 		

		first = self.package_version.find('.')
		second = self.package_version.find('.', first + 1)
		if first == -1 or second == -1 or first == second:
			version_dir = self.package_version
		else:
			version_dir = self.package_version[:second]
		self.package_dl_url = 'http://download.gnome.org/sources/%s/%s/' % (self.package_name.lower(), 
										    version_dir)

	def get_bugzilla_info(self):
		query = 'http://bugzilla.gnome.org/browse.cgi?product=%s' % (self.package_module)
		f = urllib.urlopen(query)
		s = f.read()
		f.close()

		s1 = '<p><i>'
		i = s.find(s1)
		start = i + len(s1)
		s2 = '</i></p>'
		end = s.find(s2, i + 1)
		self.description = s[start:end]

		s1 = "GNOME SVN"
		i = s.find(s1)
		s1 = "href"
		i = s.find(s1, i)        
		start = i + 6
		s2 = '">'
		end = s.find(s2, start)
		self.url = s[start:end]
	
	def get_commits(self):
		bugs = ''
		co = None
		changes = exec_cmd ("git-log " + prev_tag + "..")
        	for line in changes.splitlines(1):
        		if line.startswith('commit'):
        			if co != None:
        				bug = co.parse()
        				if bug:
        					if bugs != '':
        						bugs += ','
        					bugs += bug

        			co = Commit()
        			self.commits.append(co)
				p1 = line.find(' ')
				co.ref = line[p1:].strip()
        		elif line.startswith('Author:'):
        			p1 = line.find(' ')
        			p2 = line.find('<')
        			co.author = line[p1:p2].strip()
        		elif line.startswith('Date:'):
        			p1 = line.find(' ')
        			co.date = line[p1:].strip()
        		elif line.startswith('    git-svn-id:'):
        			continue
        		elif line.startswith('Merge:'):
        			continue
        		else:
				msg = line.strip()
				if msg == '':
					continue
				if msg.startswith('*'):
					p1 = msg.find(':')
					msg = msg[p1 + 1:].strip()
				elif msg.startswith('2007-') or msg.startswith('2008-'):
					continue
				if co.message != '':
					co.message += '\n'
				co.message += msg

		# Bugzilla query to use
		query = 'http://bugzilla.gnome.org/buglist.cgi?ctype=csv' \
			'&bug_status=RESOLVED,CLOSED,VERIFIED' \
			'&resolution=FIXED' \
			'&bug_id=' + bugs.replace(',', '%2c')

		f = urllib.urlopen(query)
		s = f.read()
		f.close()

		col_bug_id = -1
		col_description = -1

		reader = csv.reader(s.splitlines(1))
		header = reader.next()
		i = 0

		for col in header:
			if col == 'bug_id':
				col_bug_id = i
			if col == 'short_short_desc':
				col_description = i
			i = i + 1

		for row in reader:
			bug_number = row[col_bug_id]
			description = row[col_description]

			for co in self.commits:
				if co.bug == bug_number:
					co.summary = 'Fixed #%s, %s (%s)' % (co.bug, description, co.author)
					break

p = Project()
