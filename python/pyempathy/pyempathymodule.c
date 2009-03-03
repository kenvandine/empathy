#define NO_IMPORT_PYGOBJECT

#include <pygobject.h>

void empathy_register_classes (PyObject *d); 
void empathy_add_constants(PyObject *module, const gchar *strip_prefix);
DL_EXPORT(void) initempathy(void);
extern PyMethodDef empathy_functions[];

DL_EXPORT(void)
initempathy(void)
{
	PyObject *m, *d;

	init_pygobject ();
	
	m = Py_InitModule ("empathy", empathy_functions);
	d = PyModule_GetDict (m);
	
	empathy_register_classes (d);
	empathy_add_constants(m, "EMPATHY_");
	
	if (PyErr_Occurred ()) {
		PyErr_Print();
		Py_FatalError ("can't initialise module empathy");
	}
}

