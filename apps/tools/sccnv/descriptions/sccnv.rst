sccnv reads input given in a supported format, converts the content to another
format and writes the output. Use the command-line option :option:`--format-list`
for a list of supported formats.


.. _sec-sccnv-formats:

Formats
=======

Different formats are supported for input and output files.

.. csv-table::
   :widths: 10, 60, 10, 10
   :header: Name, Description, Input, Output
   :align: left
   :delim: ;

   arclink    ; `Arclink XML <https://www.seiscomp.de/seiscomp3/doc/applications/arclink-status-xml.html>`_ ;    X    ;    X
   binary     ; :ref:`SeisComP data model <api-datamodel-python>` in binary format: low-weight and fast     ;    X    ;    X
   bson       ;                                                                                             ;    X    ;    X
   bson-json  ;                                                                                             ;         ;    X
   csv        ; comma-separated values                                                                      ;         ;    X
   hyp71sum2k ; Hypo71 format                                                                               ;         ;    X
   ims10      ;                                                                                             ;         ;    X
   json       ; :cite:t:`json` format                                                                       ;    X    ;    X
   qml1.2     ; :term:`QuakeML` format                                                                      ;   \*    ;    X
   qml1.2rt   ; :term:`QuakeML` real time (RT) format                                                       ;   \*    ;    X
   scdm0.51   ;                                                                                             ;    X    ;    X
   scml       ; SeisComP XML (:term:`SCML`) - :ref:`SCML API <api-datamodel-python>`                        ;    X    ;    X
   trunk      ; same as scml, classic way of stating this format                                            ;    X    ;    X
   vbson      ;                                                                                             ;    X    ;    X

**\***: The conversion from files in QuakeML format is not supported by sccnv
but can be realized by system tools. Read section :ref:`sec-sccnv-quakeml` for
details and instructions.

Input and output are given as ``[format:][file]``. Without file, stdin or stdout
is used. Without format, the format of an input file is detected from its
content: SCML, Arclink XML, JSON and vbinary are recognized. Files
which cannot be detected are read as SCML. Output is written in SCML unless
another format is given.

The part in front of the first colon is only considered as format if it is the
name of a supported format. Hence, input file names containing colons, e.g.,
time stamps, can be given without format. Output file names containing colons
require stating the format or the path, e.g., ``scml:2024-01-01T10:00:00.xml``
or ``./2024-01-01T10:00:00.xml``.

Input in formats provided by plugins requires loading the plugin, e.g.
FDSN StationXML with ``--plugins fdsnxml``.


.. _sec-sccnv-quakeml:

QuakeML
-------

:term:`QuakeML` is used in a variety of flavors involving, e.g.,

* Using non-standard objects,
* PublicID references which are not globally unique,
* Missing references to parent objects,
* Missing creationInfo parameters.

The ability to convert from QuakeML to :term:`SCML` is thus limited and it
depends on the parameters provided with the input QuakeML file.

However, XSLT stylesheets are provided for mapping the parameters. The files
are located in :file:`@DATADIR@/xml/[version]/` for different |scname| data schema
versions. The stylesheet files provide information on the mapping and on
limitations as well as examples on their application.

.. note::

   You may find out about the |scname| data schema version using modules along
   with the command-line option `-V`, e.g.,

   .. code-block:: sh

      $ sccnv -V

The style sheets can be used along with other stylesheet converter tools provided
by your system, e.g., :program:`xalan` or :program:`xsltproc`. Examples are given
in section :ref:`sec-sccnv-examples`.


.. _sec-sccnv-examples:

Examples
========

* Print the list of supported formats:

  .. code-block:: sh

     $ sccnv --format-list

* Convert an  event parameter file in :term:`SCML` format to :term:`QuakeML` and
  store the content in a file:

  .. code-block:: sh

     $ sccnv -i seiscomp.xml -o qml1.2:quakeml.xml

* Convert an event parameter file in JSON format, detected automatically, to
  formatted :term:`SCML` and write the content to stdout:

  .. code-block:: sh

     $ sccnv -i event.json -f

* Convert from stdin to stdout, e.g., as part of a pipe:

  .. code-block:: sh

     $ cat event.json | sccnv -i json: > event.xml

* Convert an inventory file in Arclink XML format to :term:`SCML` and store the
  content in a file:

  .. code-block:: sh

     $ sccnv -i arclink:Package_inventory.xml -o inventory.sc.xml

* Convert an event parameter file in :term:`SCML` format to ims1.0 and store the
  content in a file:

  .. code-block:: sh

     $ sccnv -i scml:event.xml -o ims10:event.ims

* Convert QuakeML in version 1.2 to SCML in data schema version 0.12:

  .. code-block:: sh

     $ xsltproc $SEISCOMP_ROOT/share/xml/0.12/quakeml_1.2__sc3ml_0.12.xsl file.quakeml > file_sc.xml
