#!/usr/bin/env python

import gtk
import empathy
import empathygtk

class HelloWorld:

    def destroy(self, widget, data=None):
        print "destroy signal occurred"
        gtk.main_quit()

    def __init__(self):
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.connect("destroy", self.destroy)

        manager = empathy.ContactManager()
        store = empathygtk.ContactListStore(manager)
        view = empathygtk.ContactListView(store, 0)

        self.window.add (view)
        view.show()
        self.window.show()

    def main(self):
        gtk.main()

if __name__ == "__main__":
    hello = HelloWorld()
    hello.main()

