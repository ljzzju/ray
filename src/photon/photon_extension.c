#include <Python.h>

#include "common_extension.h"
#include "photon_client.h"
#include "task.h"

PyObject *PhotonError;

// clang-format off
typedef struct {
  PyObject_HEAD
  photon_conn *photon_connection;
} PyPhotonClient;
// clang-format on

static int PyPhotonClient_init(PyPhotonClient *self,
                               PyObject *args,
                               PyObject *kwds) {
  char *socket_name;
  if (!PyArg_ParseTuple(args, "s", &socket_name)) {
    return -1;
  }
  self->photon_connection = photon_connect(socket_name);
  return 0;
}

static void PyPhotonClient_dealloc(PyPhotonClient *self) {
  free(((PyPhotonClient *) self)->photon_connection);
  Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *PyPhotonClient_submit(PyObject *self, PyObject *args) {
  PyObject *py_task;
  if (!PyArg_ParseTuple(args, "O", &py_task)) {
    return NULL;
  }
  photon_submit(((PyPhotonClient *) self)->photon_connection,
                ((PyTask *) py_task)->spec);
  Py_RETURN_NONE;
}

// clang-format off
static PyObject *PyPhotonClient_get_task(PyObject *self) {
  task_spec *task_spec;
  /* Drop the global interpreter lock while we get a task because
   * photon_get_task may block for a long time. */
  Py_BEGIN_ALLOW_THREADS
  task_spec = photon_get_task(((PyPhotonClient *) self)->photon_connection);
  Py_END_ALLOW_THREADS
  return PyTask_make(task_spec);
}
// clang-format on

static PyObject *PyPhotonClient_reconstruct_object(PyObject *self,
                                                   PyObject *args) {
  object_id object_id;
  if (!PyArg_ParseTuple(args, "O&", &PyObjectToUniqueID, &object_id)) {
    return NULL;
  }
  photon_reconstruct_object(((PyPhotonClient *) self)->photon_connection,
                            object_id);
  Py_RETURN_NONE;
}

static PyMethodDef PyPhotonClient_methods[] = {
    {"submit", (PyCFunction) PyPhotonClient_submit, METH_VARARGS,
     "Submit a task to the local scheduler."},
    {"get_task", (PyCFunction) PyPhotonClient_get_task, METH_NOARGS,
     "Get a task from the local scheduler."},
    {"reconstruct_object", (PyCFunction) PyPhotonClient_reconstruct_object,
     METH_VARARGS, "Ask the local scheduler to reconstruct an object."},
    {NULL} /* Sentinel */
};

static PyTypeObject PyPhotonClientType = {
    PyVarObject_HEAD_INIT(NULL, 0)       /* ob_size */
    "photon.PhotonClient",               /* tp_name */
    sizeof(PyPhotonClient),              /* tp_basicsize */
    0,                                   /* tp_itemsize */
    (destructor) PyPhotonClient_dealloc, /* tp_dealloc */
    0,                                   /* tp_print */
    0,                                   /* tp_getattr */
    0,                                   /* tp_setattr */
    0,                                   /* tp_compare */
    0,                                   /* tp_repr */
    0,                                   /* tp_as_number */
    0,                                   /* tp_as_sequence */
    0,                                   /* tp_as_mapping */
    0,                                   /* tp_hash */
    0,                                   /* tp_call */
    0,                                   /* tp_str */
    0,                                   /* tp_getattro */
    0,                                   /* tp_setattro */
    0,                                   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                  /* tp_flags */
    "PhotonClient object",               /* tp_doc */
    0,                                   /* tp_traverse */
    0,                                   /* tp_clear */
    0,                                   /* tp_richcompare */
    0,                                   /* tp_weaklistoffset */
    0,                                   /* tp_iter */
    0,                                   /* tp_iternext */
    PyPhotonClient_methods,              /* tp_methods */
    0,                                   /* tp_members */
    0,                                   /* tp_getset */
    0,                                   /* tp_base */
    0,                                   /* tp_dict */
    0,                                   /* tp_descr_get */
    0,                                   /* tp_descr_set */
    0,                                   /* tp_dictoffset */
    (initproc) PyPhotonClient_init,      /* tp_init */
    0,                                   /* tp_alloc */
    PyType_GenericNew,                   /* tp_new */
};

static PyMethodDef photon_methods[] = {
    {"check_simple_value", check_simple_value, METH_VARARGS,
     "Should the object be passed by value?"},
    {"compute_put_id", compute_put_id, METH_VARARGS,
     "Return the object ID for a put call within a task."},
    {NULL} /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "libphoton",                         /* m_name */
    "A module for the local scheduler.", /* m_doc */
    0,                                   /* m_size */
    photon_methods,                      /* m_methods */
    NULL,                                /* m_reload */
    NULL,                                /* m_traverse */
    NULL,                                /* m_clear */
    NULL,                                /* m_free */
};
#endif

#if PY_MAJOR_VERSION >= 3
#define INITERROR return NULL
#else
#define INITERROR return
#endif

#ifndef PyMODINIT_FUNC /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

#if PY_MAJOR_VERSION >= 3
#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#else
#define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
#endif

MOD_INIT(libphoton) {
  if (PyType_Ready(&PyTaskType) < 0) {
    INITERROR;
  }

  if (PyType_Ready(&PyObjectIDType) < 0) {
    INITERROR;
  }

  if (PyType_Ready(&PyPhotonClientType) < 0) {
    INITERROR;
  }

#if PY_MAJOR_VERSION >= 3
  PyObject *m = PyModule_Create(&moduledef);
#else
  PyObject *m = Py_InitModule3("libphoton", photon_methods,
                               "A module for the local scheduler.");
#endif

  init_pickle_module();

  Py_INCREF(&PyTaskType);
  PyModule_AddObject(m, "Task", (PyObject *) &PyTaskType);

  Py_INCREF(&PyObjectIDType);
  PyModule_AddObject(m, "ObjectID", (PyObject *) &PyObjectIDType);

  Py_INCREF(&PyPhotonClientType);
  PyModule_AddObject(m, "PhotonClient", (PyObject *) &PyPhotonClientType);

  char photon_error[] = "photon.error";
  PhotonError = PyErr_NewException(photon_error, NULL, NULL);
  Py_INCREF(PhotonError);
  PyModule_AddObject(m, "photon_error", PhotonError);

#if PY_MAJOR_VERSION >= 3
  return m;
#endif
}
