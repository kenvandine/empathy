#!/usr/bin/env python

import os
import re
import urllib
import csv
import datetime
from string import Template
from optparse import OptionParser

last_tag_patern = 'EMPATHY_2_26*'
username = 'xclaesse'
upload_server = 'master.gnome.org'
template = '''\
$name $version is now available for download from:
$download

$md5sums

What is it?
===========
$about

You can visit the project web site:
$website

What's New?
===========
$news

$footer'''

class Bug:
	number = ''
	author = ''

class Project:
	def __init__(self):
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
		tags_str = self.exec_cmd('git tag -l %s' % (last_tag_patern))
		tags = tags_str.splitlines()
		self.last_tag = tags[len(tags)-1]

	def exec_cmd(self,cmd):
		return os.popen(cmd).read()

	def get_news(self):
		f = open ('NEWS', 'r')
		s = f.read()
		f.close()
		start = s.find ('NEW in '+ self.package_version)
		if start != -1:
			start = s.find ('\n', start) + 1
			start = s.find ('\n', start) + 1
	        	end = s.find ('NEW in', start) - 1
		        return s[start:end].strip()

	def get_md5sums(self):
		md5sums = ''

		cmd = 'md5sum %s-%s.tar.gz' % (self.package_name.lower(), self.package_version)
		md5sums += self.exec_cmd(cmd)

		cmd = 'md5sum %s-%s.tar.bz2' % (self.package_name.lower(), self.package_version)
		md5sums += self.exec_cmd(cmd).strip()

		return md5sums

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
		description = s[start:end]

		s1 = "GNOME SVN"
		i = s.find(s1)
		s1 = "href"
		i = s.find(s1, i)        
		start = i + 6
		s2 = '">'
		end = s.find(s2, start)
		project_url = s[start:end]

		return (description, project_url)

	def get_release_notes(self):
		name = self.package_name
		version = self.package_version
		download = self.package_dl_url
		md5sums = self.get_md5sums()
		(about, website) = self.get_bugzilla_info()
		news = self.get_news()
		footer = '%s\n%s team' % (datetime.date.today().strftime('%d %B %Y'),\
					  self.package_name)

		t = Template(template)
		return t.substitute(locals())
	
	def get_translations(self):
		self.translations = ''
		files_str = self.exec_cmd("ls po/*.po")
		files = files_str.splitlines()
		for f in files:
			lang = f[3:len(f)-3]
			commit_str = self.exec_cmd("git log %s..  %s" % (self.last_tag, f))
			if commit_str == '':
				continue

			authors = ''
			for line in commit_str.splitlines():
				if line.startswith('Author:'):
	        			p1 = line.find(' ')
        				p2 = line.find('<')
        				author = line[p1:p2].strip()

					if authors.find(author) != -1:
						continue
        				if authors != '':
        					authors += ", "
        				authors += author

			self.translations += "Updated %s Translation (%s)\n" % (lang, authors)

	def get_bugs(self):
		query = 'http://bugzilla.gnome.org/buglist.cgi?' \
			'ctype=csv&product=empathy&' \
			'bug_status=RESOLVED,CLOSED,VERIFIED&resolution=FIXED&' \
			'chfieldfrom=%s&chfieldto=Now' % ('2009-04-13')
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

		self.bugs = ''
		for row in reader:
			bug_number = row[col_bug_id]
			description = row[col_description]
			self.bugs += ' - Fixed #%s, %s\n' % (bug_number, description)

	def make_tag(self):
		new_tag = self.package_name.upper() + '_' +\
			  self.package_version.replace('.', '_')
		self.exec_cmd('git tag -m "Tagged for release %s." %s' % ( self.package_version, new_tag))

	def generate_news(self):
		self.get_translations()
		self.get_bugs()
		news = 'NEW in '+ self.package_version + '\n==============\n'
		if self.bugs != '':
			news += 'Bugs fixed:\n' + self.bugs + '\n'
		if self.translations != '':
			news += 'Translations:\n' + self.translations + '\n'

		return news

	def write_news(self):
		news = self.generate_news()

		f = open ('/tmp/NEWS', 'w')
		s = f.write(news)
		f.close()

		self.exec_cmd('cat NEWS >> /tmp/NEWS')
		self.exec_cmd('mv /tmp/NEWS .')

	def upload_tarball(self):
		tarball = '%s-%s.tar.gz' % (self.package_name.lower(), self.package_version)

		cmd = 'scp %s %s@%s:' % (tarball, username, upload_server)
		self.exec_cmd(cmd)

		cmd = 'ssh %s@%s install-module -u %s' % (username, upload_server, tarball)
		self.exec_cmd(cmd)

	def send_email(self):
		notes = self.get_release_notes()
		cmd = 'xdg-email ' \
		      ' --cc telepathy@lists.freedesktop.org' \
		      ' --subject "ANNOUNCE: Empathy %s"' \
		      ' --body "%s"' \
		      ' gnome-announce-list@gnome.org' % (self.package_version,
		      					  notes.replace('"', '\\"'))
		self.exec_cmd(cmd)

	def release(self):
		self.make_tag()
		self.upload_tarball()
		self.send_email()

if __name__ == '__main__':
	p = Project()
	parser = OptionParser()
	parser.add_option("-n", "--print-news", action="store_true",\
		dest="print_news", help="Generate and print news")
	parser.add_option("-p", "--print-notes", action="store_true",\
		dest="print_notes", help="Generate and print the release notes")
	parser.add_option("-w", "--write-news", action="store_true",\
		dest="write_news", help="Generate and write news into the NEWS file")
	parser.add_option("-r", "--release", action="store_true",\
		dest="release", help="Release the tarball")
	parser.add_option("-e", "--email", action="store_true",\
		dest="email", help="Send the release announce email")

	(options, args) = parser.parse_args ()
	if (options.print_news):
		print p.generate_news ()
	if (options.print_notes):
		print p.get_release_notes ()
	if (options.write_news):
		p.write_news ()
	if (options.release):
		p.release ()
	if (options.email):
		p.send_email ()

