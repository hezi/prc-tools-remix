/* build-prc.cpp: build a .prc from a pile of files.

   Copyright 2002, 2003, 2004 John Marshall.
   Portions copyright 1998-2001 Palm, Inc. or its subsidiaries.

   This is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program, build-prc v2.0, follows in the footsteps of obj-res and
   build-prc, the source code of which contains the following notices:

 * obj-res.c:  Dump out .prc compatible binary resource files from an object
 *
 * (c) 1996, 1997 Dionne & Associates
 * jeff@ryeham.ee.ryerson.ca
 *
 * This is Free Software, under the GNU Public Licence v2 or greater.
 *
 * Relocation added March 1997, Kresten Krab Thorup
 * krab@california.daimi.aau.dk

 * ptst.c:  build a .prc from a pile of files.
 *
 * (c) 1996, Dionne & Associates
 * (c) 1997, The Silver Hammer Group Ltd.
 * This is Free Software, under the GNU Public Licence v2 or greater.
 */

#include <map>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "getopt.h"
#include "utils.h"
#include "def.h"
#include "binres.hpp"
#include "pfd.hpp"
#include "pfdio.hpp"

void
usage () {
  printf ("Usage: Old-style: %s [options] outfile.prc dbname crid file...\n"
	  "       New-style: %s [options] file...\n", progname, progname);

  printf ("Files may be .bin/.grc, .prc/.ro, "
	  ".def (new-style only), or linked executables\n");

  // Commented out options are intended but not yet implemented
  printf ("Options:\n");
  propt ("-o FILE, --output FILE",
	 "Set output .prc file name (new-style only)");
  propt ("-l, -L", "Build a GLib or a system library respectively");
  propt ("-H, --hack", "Build a HackMaster hack");
  propt ("-a FILE, --appinfo FILE", "Add an AppInfo block");
  propt ("-s FILE, --sortinfo FILE", "Add a SortInfo block");
  propt ("-t TYPE, --type TYPE", "Set database type");
  propt ("-c CRID, --creator CRID", "Set database creator (new-style only)");
  propt ("-n NAME, --name NAME", "Set database name (new-style only)");
  propt ("-m NUM, --modification-number NUM",
	 "Set database modification number");
  propt ("-v NUM, --version-number NUM", "Set database version number");
  propt ("--readonly, --read-only, --appinfodirty, --appinfo-dirty, --backup,",
	 NULL);
  propt ("--oktoinstallnewer, --ok-to-install-newer, --resetafterinstall,",
	 NULL);
  propt ("--reset-after-install, --copyprevention, --copy-prevention,", NULL);

  propt ("--stream, --hidden, --launchabledata, --launchable-data,",
	 NULL);
  propt ("--recyclable, --bundle",
	 "Set database attributes");
  propt ("-z N, --compress-data N",
	 "Set data resource compression method (0--7)");
  propt ("--no-check-header", "Suppress database header validity warnings");
  propt ("--no-check-resources",
	 "Suppress diagnosis of missing vital resources");
  propt ("--no-check", "Suppress all checking of the generated database");
  // propt ("-x, --provenance", "Output resource cross-reference");
  }

enum {
  OPTION_READONLY = 150,
  OPTION_APPINFO_DIRTY,
  OPTION_BACKUP,
  OPTION_OK_TO_INSTALL_NEWER,
  OPTION_RESET_AFTER_INSTALL,
  OPTION_COPY_PREVENTION,
  OPTION_STREAM,
  OPTION_HIDDEN,
  OPTION_LAUNCHABLE_DATA,
  OPTION_RECYCLABLE,
  OPTION_BUNDLE,
  OPTION_NO_CHECK_HEADER,
  OPTION_NO_CHECK_RESOURCES,
  OPTION_NO_CHECK,
  OPTION_HELP,
  OPTION_VERSION
  };

static const char shortopts[] = "o:lLHa:s:t:c:n:m:v:z:";

static struct option longopts[] = {
  { "output", required_argument, NULL, 'o' },
  { "appinfo", required_argument, NULL, 'a' },
  { "sortinfo", required_argument, NULL, 's' },
  { "type", required_argument, NULL, 't' },
  { "creator", required_argument, NULL, 'c' },
  { "name", required_argument, NULL, 'n' },
  { "modification-number", required_argument, NULL, 'm' },
  { "version-number", required_argument, NULL, 'v' },
  { "provenance", no_argument, NULL, 'x' },
  { "compress-data", required_argument, NULL, 'z' },
  { "hack", no_argument, NULL, 'H' },

  { "readonly", no_argument, NULL, OPTION_READONLY },
  { "read-only", no_argument, NULL, OPTION_READONLY },
  { "appinfodirty", no_argument, NULL, OPTION_APPINFO_DIRTY },
  { "appinfo-dirty", no_argument, NULL, OPTION_APPINFO_DIRTY },
  { "backup", no_argument, NULL, OPTION_BACKUP },
  { "oktoinstallnewer", no_argument, NULL, OPTION_OK_TO_INSTALL_NEWER },
  { "ok-to-install-newer", no_argument, NULL, OPTION_OK_TO_INSTALL_NEWER },
  { "reset-after-install", no_argument, NULL, OPTION_RESET_AFTER_INSTALL },
  { "resetafterinstall", no_argument, NULL, OPTION_RESET_AFTER_INSTALL },
  { "copy-prevention", no_argument, NULL, OPTION_COPY_PREVENTION },
  { "copyprevention", no_argument, NULL, OPTION_COPY_PREVENTION },
  { "stream", no_argument, NULL, OPTION_STREAM },
  { "hidden", no_argument, NULL, OPTION_HIDDEN },
  { "launchabledata", no_argument, NULL, OPTION_LAUNCHABLE_DATA },
  { "launchable-data", no_argument, NULL, OPTION_LAUNCHABLE_DATA },
  { "recyclable", no_argument, NULL, OPTION_RECYCLABLE },
  { "bundle", no_argument, NULL, OPTION_BUNDLE },
  { "no-check-header", no_argument, NULL, OPTION_NO_CHECK_HEADER },
  { "no-check-resources", no_argument, NULL, OPTION_NO_CHECK_RESOURCES },
  { "no-check", no_argument, NULL, OPTION_NO_CHECK },

  { "help", no_argument, NULL, OPTION_HELP },
  { "version", no_argument, NULL, OPTION_VERSION },
  { NULL, no_argument, NULL, 0 }
  };


typedef std::map<ResKey, std::string> ResourceProvenance;

static ResourceDatabase db;
static ResourceProvenance prov;
static struct binary_file_info bininfo;
static void (*update_bininfo) (const ResourceDatabase&);
static void (*check_resources) (const char*, const ResourceDatabase&,
				const struct binary_file_info&);


void
add_resource (const char* origin, const ResKey& key, const Datablock& data) {
  ResourceProvenance::const_iterator prev_supplier = prov.find (key);
  if (prev_supplier == prov.end()) {
    db[key] = data;
    prov[key] = origin;
    }
  else
    warning ("[%s] resource '%.4s' #%u already obtained from '%s'",
	     origin, key.type, key.id, (*prev_supplier).second.c_str());
  }


static void
update_bininfo_maincode_id (const ResourceDatabase& db) {
  while (db.find (bininfo.maincode) != db.end())
    bininfo.maincode.id++;
  }


static void
check_maincode (const char* fname, const ResourceDatabase& db,
		const struct binary_file_info& info) {
  if (db.find (info.maincode) == db.end())
    error ("[%s] resource '%.4s' #%u is missing",
	   fname, info.maincode.type, info.maincode.id);
  }

struct range {
  unsigned int lo, hi, n;
  };

static void
check_range (const char* fname, const struct range& r, const char* type,
	     unsigned int full_lo, unsigned int full_hi) {
  if (r.n > 0) {
    if (r.lo != full_lo)
      warning ("[%s] first '%s' resource in range #%u--%u is #%u",
	       fname, type, full_lo, full_hi, r.lo);

    unsigned int size = r.hi - r.lo + 1;
    if (r.n < size)
      warning ("[%s] '%s' resources #%u--%u are noncontiguous (%u missing)",
	       fname, type, r.lo, r.hi, size - r.n);
    }
  }

static void
check_hack (const char* fname, const ResourceDatabase& db,
	    const struct binary_file_info&) {
  struct range traps, forms;

  traps.lo = forms.lo = 0xffff;
  traps.hi = forms.hi = 0;
  traps.n = forms.n = 0;

  // Check that there is a corresponding 'code' for each 'TRAP' and
  // configuration 'tFRM', and that the traps are numbered within range

  for (ResourceDatabase::const_iterator it = db.begin(); it != db.end(); ++it) {
    unsigned int id = (*it).first.id;

    if (strncmp ((*it).first.type, "TRAP", 4) == 0) {
      if (id >= 1000 && id < 1500) {
	if (id < traps.lo)  traps.lo = id;
	if (id > traps.hi)  traps.hi = id;
	traps.n++;
	if (db.find (ResKey ("code", id)) == db.end())
	  error ("[%s] "
		 "resource 'TRAP' #%u has no corresponding 'code' resource",
		 fname, id);
	}
      else
	warning ("[%s] 'TRAP' #%u is out of its range of #1000--1499",
		 fname, id);
      }
    else if (strncmp ((*it).first.type, "tFRM", 4) == 0
	     && id >= 2000 && id <= 2999) {
      if (id < forms.lo)  forms.lo = id;
      if (id > forms.hi)  forms.hi = id;
      forms.n++;
      if (db.find (ResKey ("code", id)) == db.end())
	error ("[%s] Hack form #%u has no event handler 'code' resource",
	       fname, id);
      }
    }

  // and that each forms a contiguous sequence from the base of its range

  check_range (fname, traps, "TRAP", 1000, 1499);
  check_range (fname, forms, "tFRM", 2000, 2999);

  // and that at least one trap and the name is present

  if (traps.n == 0)
    error ("[%s] Hack database contains no 'TRAP' resources", fname);

  if (db.find (ResKey ("tAIN", 3000)) == db.end())
    warning ("[%s] resource 'tAIN' #3000 (Hack name) is missing", fname);
  }


struct error_with_fname {
  const char* format;
  const char* fname;
  error_with_fname (const char* format0, const char* fname0)
    : format (format0), fname (fname0) {}
  };

Datablock
slurp_file_as_datablock (const char* fname) {
  long length;
  FILE *f;

  if ((length = file_length(fname)) >= 0 && (f = fopen (fname, "rb")) != NULL) {
    Datablock block (length);
    size_t length_read = fread (block.writable_contents(), 1, length, f);
    fclose (f);
    if (length_read != size_t(length))
      throw error_with_fname ("error reading '%s': @P", fname);
    return block;
    }
  else
    throw error_with_fname ("can't open '%s': @P", fname);
  }


/* The aim of this priority mechanism is to correctly initialise settings
   from default values, the definition file, the old-style command line,
   and command line options, with settings from the latter overriding the
   former.  The following state is under the control of this mechanism:

	db	name, type, creator, all 9 attributes, version, modnum
	bininfo	maincode, emit_appl_extras, emit_data, stack_size  */

enum priority_level {
  default_pri, def_default_pri, def_specific_pri, old_cli_pri, option_pri
  };

static std::map<const void*, priority_level> priority;

template <class T> static bool
superior (T& key, priority_level pri) {
  if (pri >= priority[&key]) {
    priority[&key] = pri;
    return true;
    }
  else
    return false;
  }

static bool
set_db_kind (database_kind kind, priority_level pri) {
  ResKey maincode;
  bool emit_appl_extras, emit_data;
  void (*update) (const ResourceDatabase&);
  void (*check) (const char*, const ResourceDatabase&,
		 const struct binary_file_info&);

  switch (kind) {
  case DK_APPLICATION:
    maincode = ResKey ("code", 1);
    emit_appl_extras = true;
    emit_data = true;
    update = NULL;
    check = check_maincode;
    break;

  case DK_GLIB:
    maincode = ResKey ("GLib", 0);
    emit_appl_extras = false;
    emit_data = true;
    update = NULL;
    check = check_maincode;
    break;

  case DK_SYSLIB:
    maincode = ResKey ("libr", 0);
    emit_appl_extras = false;
    emit_data = false;
    update = NULL;
    check = check_maincode;
    break;

  case DK_HACK:
    maincode = ResKey ("code", 1000);
    emit_appl_extras = false;
    emit_data = false;
    /* Hacks contain several patch resources.  Corresponding binary files
       that don't have an explicit desired resource id depend on us to find
       them an id that hasn't been used yet.  */
    update = update_bininfo_maincode_id;
    check = check_hack;
    break;

  case DK_GENERIC:
  default:
    maincode = ResKey ("code", 0);
    emit_appl_extras = false;
    emit_data = false;
    update = NULL;
    check = NULL;
    break;
    }

  if (superior (bininfo.emit_appl_extras, pri))
    bininfo.emit_appl_extras = emit_appl_extras;
  if (superior (bininfo.emit_data, pri))
    bininfo.emit_data = emit_data;
  if (superior (update_bininfo, pri))
    update_bininfo = update;
  if (superior (check_resources, pri))
    check_resources = check;
  if (superior (bininfo.maincode, pri)) {
    bininfo.maincode = maincode;
    return true;
    }
  else
    return false;
  }

// I'm not sure I really understand how to do this, but we'll try:
extern "C" {

// Some of these functions need the .def file's name passed down to them so
// that they can update the provenance structure.
static const char* deffname;

static void
db_header (database_kind kind, const struct database_header* h) {
  if (!set_db_kind (kind, def_default_pri))
    error ("'-l'/'-L'/'-H'/'--hack' options conflict with definition file");

  if (superior (db.name, def_default_pri))
    strncpy (db.name, h->name, 32);
  if (superior (db.type, def_default_pri))
    strncpy (db.type, h->type, 4);
  if (superior (db.creator, def_default_pri))
    strncpy (db.creator, h->creator, 4);

  if (superior (db.readonly, def_default_pri))
    db.readonly = h->readonly;
  if (superior (db.appinfo_dirty, def_default_pri))
    db.appinfo_dirty = h->appinfo_dirty;
  if (superior (db.backup, def_default_pri))
    db.backup = h->backup;
  if (superior (db.ok_to_install_newer, def_default_pri))
    db.ok_to_install_newer = h->ok_to_install_newer;
  if (superior (db.reset_after_install, def_default_pri))
    db.reset_after_install = h->reset_after_install;
  if (superior (db.copy_prevention, def_default_pri))
    db.copy_prevention = h->copy_prevention;
  if (superior (db.stream, def_default_pri))
    db.stream = h->stream;
  if (superior (db.hidden, def_default_pri))
    db.hidden = h->hidden;
  if (superior (db.launchable_data, def_default_pri))
    db.launchable_data = h->launchable_data;
  if (superior (db.recyclable, def_default_pri))
    db.recyclable = h->recyclable;
  if (superior (db.bundle, def_default_pri))
    db.bundle = h->bundle;

  if (superior (db.version, def_default_pri))
    db.version = h->version;
  if (superior (db.modnum, def_default_pri))
    db.modnum = h->modnum;
  }

static void
multicode_section (const char* secname) {
  unsigned int id = 2 + bininfo.extracode.size();

  if (bininfo.extracode.find (secname) == bininfo.extracode.end())
    bininfo.extracode[secname] = ResKey ("code", id);
  else
    error ("[%s] section '%s' duplicated", deffname, secname);
  }

static void
data (int allow) {
  if (superior (bininfo.emit_data, def_specific_pri))
    bininfo.emit_data = allow;
  }

static void
stack (unsigned long stack_size) {
  if (superior (bininfo.stack_size, def_specific_pri))
    bininfo.stack_size = stack_size;
  }

static void
version_resource (unsigned long resid, const char* text) {
  // tver resources are null-terminated
  long size = strlen (text) + 1;
  Datablock block (size);
  memcpy (block.writable_contents(), text, size);
  add_resource (deffname, ResKey ("tver", resid), block);
  }

}


enum file_type {
  FT_UNKNOWN,
  FT_RAW,	/* .grc or .bin */
  FT_BFD,	/* executable (no extension) */
  FT_PRC,	/* .prc */
  FT_RO,	/* .ro */
  FT_DEF	/* .def */
  };

enum file_type
file_type (const char* fname) {
  char ext[FILENAME_MAX];
  const char* dot = strrchr(fname, '.');

  strcpy (ext, (dot)? dot+1 : "");
  for (char* s = ext; *s; s++)
    *s = tolower (*s);

  return   (strcmp (ext, "grc") == 0 || strcmp (ext, "bin") == 0)?  FT_RAW
	 : (strcmp (ext, "prc") == 0)? FT_PRC
	 : (strcmp (ext, "ro")  == 0)? FT_RO
	 : (strcmp (ext, "def") == 0)? FT_DEF
	 : FT_BFD;
  }


int
main (int argc, char** argv) {
  bool work_desired = true;
  int c;

  char* output_fname = NULL;
  bool check_header = true;

  set_progname (argv[0]);

  set_db_kind (DK_APPLICATION, default_pri);
  init_database_header (&db);
  strncpy (db.type, "appl", 4);
  db.version = 1;

  bininfo.stack_size = 4096;
  bininfo.force_rloc = false;
  bininfo.data_compression = 0;

  while ((c = getopt_long (argc, argv, shortopts, longopts, NULL)) >= 0)
    try {
    switch (c) {
    case 'o':
      output_fname = optarg;
      break;

    case 'l':
      if (superior (db.type, option_pri))  strncpy (db.type, "GLib", 4);
      set_db_kind (DK_GLIB, option_pri);
      break;

    case 'L':
      if (superior (db.type, option_pri))  strncpy (db.type, "libr", 4);
      set_db_kind (DK_SYSLIB, option_pri);
      break;

    case 'H':
      if (superior (db.type, option_pri))  strncpy (db.type, "HACK", 4);
      set_db_kind (DK_HACK, option_pri);
      break;

    case 'a':
      db.appinfo = slurp_file_as_datablock (optarg);
      break;

    case 's':
      db.sortinfo = slurp_file_as_datablock (optarg);
      break;

    case 't':
      if (superior (db.type, option_pri))  strncpy (db.type, optarg, 4);
      break;

    case 'c':
      if (superior (db.creator, option_pri))  strncpy (db.creator, optarg, 4);
      break;

    case 'n':
      if (superior (db.name, option_pri))  strncpy (db.name, optarg, 32);
      break;

    case 'm':
      if (superior (db.modnum, option_pri))
	db.modnum = strtoul (optarg, NULL, 0);
      break;

    case 'v':
      if (superior (db.version, option_pri))
	db.version = strtoul (optarg, NULL, 0);
      break;

    case 'z':
      bininfo.data_compression = strtoul (optarg, NULL, 0);
      break;

    case OPTION_READONLY:
      if (superior (db.readonly, option_pri))  db.readonly = true;
      break;

    case OPTION_APPINFO_DIRTY:
      if (superior (db.appinfo_dirty, option_pri))  db.appinfo_dirty = true;
      break;

    case OPTION_BACKUP:
      if (superior (db.backup, option_pri))  db.backup = true;
      break;

    case OPTION_OK_TO_INSTALL_NEWER:
      if (superior (db.ok_to_install_newer, option_pri))
	db.ok_to_install_newer = true;
      break;

    case OPTION_RESET_AFTER_INSTALL:
      if (superior (db.reset_after_install, option_pri))
	db.reset_after_install = true;
      break;

    case OPTION_COPY_PREVENTION:
      if (superior (db.copy_prevention, option_pri))  db.copy_prevention = true;
      break;

    case OPTION_STREAM:
      if (superior (db.stream, option_pri))  db.stream = true;
      break;

    case OPTION_HIDDEN:
      if (superior (db.hidden, option_pri))  db.hidden = true;
      break;

    case OPTION_LAUNCHABLE_DATA:
      if (superior (db.launchable_data, option_pri))  db.launchable_data = true;
      break;

    case OPTION_RECYCLABLE:
      if (superior (db.recyclable, option_pri))  db.recyclable = true;
      break;

    case OPTION_BUNDLE:
      if (superior (db.bundle, option_pri))  db.bundle = true;
      break;

    case OPTION_NO_CHECK_HEADER:
      check_header = false;
      break;

    case OPTION_NO_CHECK_RESOURCES:
      if (superior (check_resources, option_pri))  check_resources = NULL;
      break;

    case OPTION_NO_CHECK:
      check_header = false;
      if (superior (check_resources, option_pri))  check_resources = NULL;
      break;

    case OPTION_HELP: {
      usage();
      printf ("Supported binary targets:\n");
      const char** targets = binary_file_targets ();
      for (const char** t = targets; *t; t++)
	printf ("%s%s", (t == targets)? "  " : ", ", *t);
      printf ("\n");
      free (targets);
      work_desired = false;
      }
      break;

    case OPTION_VERSION:
      print_version ("build-prc", "Jpg");
      work_desired = false;
      break;
      }
    } catch (const error_with_fname& err) {
      error (err.format, err.fname);
      }

  if (!work_desired)
    return EXIT_SUCCESS;

  enum file_type first = (optind < argc)? file_type (argv[optind]) : FT_UNKNOWN;

  if (first == FT_PRC && !output_fname) {  // Old-style arguments
    if (argc - optind >= 3) {
      output_fname = argv[optind++];

      if (file_exists (argv[optind]) && file_exists (argv[optind + 1]))
	warning ("database name ('%s') and creator id ('%s') arguments also "
		 "denote existing files (did you really intend to use "
		 "old-style syntax?)", argv[optind], argv[optind + 1]);

      if (superior (db.name, old_cli_pri))
	strncpy (db.name, argv[optind], 32);
      else
	error ("'-n' option conflicts with old-style arguments");
      optind++;

      if (superior (db.creator, old_cli_pri))
	strncpy (db.creator, argv[optind], 4);
      else
	error ("'-c' option conflicts with old-style arguments");
      optind++;
      }
    else {
      usage();
      nerrors++;
      }
    }
  else {  // New-style arguments
    if (!output_fname) {
      if (optind < argc) {
	static char fname[FILENAME_MAX];
	strcpy (fname, argv[optind]);
	output_fname = basename_with_changed_extension (fname, ".prc");
	}
      else {
	/* We need either an -o option or an input filename to construct
	   an output filename.  In the absence of both, just give the usage
	   because the most likely case is "build-prc" by itself without
	   any arguments at all.  */
	usage();
	nerrors++;
	}
      }
    }

  if (first == FT_DEF) {
    struct def_callbacks def_funcs = default_def_callbacks;
    def_funcs.db_header = db_header;
    def_funcs.multicode_section = multicode_section;
    def_funcs.data = data;
    def_funcs.stack = stack;
    def_funcs.version_resource = version_resource;
    deffname = argv[optind++];
    read_def_file (deffname, &def_funcs);
    }

  if (nerrors)
    return EXIT_FAILURE;

  for (int i = optind; i < argc; i++)
    try {
    switch (file_type (argv[i])) {
    case FT_DEF:
      error ((first == FT_DEF)? "only one definition file may be used"
			      : "the definition file must come first");
      break;

    case FT_RAW: {
      Datablock block = slurp_file_as_datablock (argv[i]);
      char buffer[FILENAME_MAX];
      strcpy (buffer, argv[i]);
      char* key = basename_with_changed_extension (buffer, NULL);
      char* dot = strchr (key, '.');
      /* A dot in the first four characters might just possibly be a very
	 strange resource type.  Any beyond there are extensions, which
	 we'll remove.  */
      if (dot && dot-key >= 4)  *dot = '\0';
      if (strlen (key) < 8) {
	while (strlen (key) < 4)  strcat (key, "x");
	while (strlen (key) < 8)  strcat (key, "0");
	warning
	  ("[%s] raw filename doesn't start with 'typeNNNN'; treated as '%s'",
	   argv[i], key);
	}
      else
	key[8] = '\0';  /* Ensure strtoul() doesn't get any extra digits.  */

      add_resource (argv[i], ResKey (key, strtoul (&key[4], NULL, 16)), block);
      }
      break;

    case FT_BFD: {
      if (update_bininfo)
	update_bininfo (db);
      ResourceDatabase bfd_db = process_binary_file (argv[i], bininfo);
      for (ResourceDatabase::const_iterator it = bfd_db.begin();
	   it != bfd_db.end();
	   ++it)
	add_resource (argv[i], (*it).first, (*it).second);
      }
      break;

    case FT_PRC:
    case FT_RO: {
      Datablock block = slurp_file_as_datablock (argv[i]);
      ResourceDatabase prc_db (block);
      for (ResourceDatabase::const_iterator it = prc_db.begin();
	   it != prc_db.end();
	   ++it)
	add_resource (argv[i], (*it).first, (*it).second);
      }
      break;

    case FT_UNKNOWN:
      warning ("[%s] ignoring unrecognized file type", argv[i]);
      break;
      }
    } catch (const char*& message) {
      error ("[%s] %s", argv[i], message);
      }
      catch (const error_with_fname& err) {
      error (err.format, err.fname);
      }

  if (nerrors == 0 && check_resources)
    check_resources (output_fname, db, bininfo);

  if (nerrors == 0 && check_header) {
    if (db.name[0] == '\0')
      warning ("creating '%s' without a name", output_fname);
    if (db.creator[0] == '\0')
      warning ("creating '%s' without a creator id", output_fname);
    }

  if (nerrors == 0) {
    FILE* f = fopen (output_fname, "wb");
    if (f) {
      time_t now = time (NULL);
      struct tm* now_tm = localtime (&now);
      db.created = db.modified = *now_tm;
      if (db.write (f))
	fclose (f);
      else {
	error ("error writing to '%s': @P", output_fname);
	fclose (f);
	remove (output_fname);
	}
      }
    else
      error ("can't write to '%s': @P", output_fname);
    }

  return (nerrors == 0)? EXIT_SUCCESS : EXIT_FAILURE;
  }
