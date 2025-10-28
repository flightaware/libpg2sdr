# Gain table data

This directory contains:

 * LNA, MIX, and VGA gain lookup tables (lna.csv, mix.csv, vga.csv);
 * a combined gain curve (sensitivity-curve.csv);
 * a script to convert the CSVs into C code suitable for use by libpg2sdr (generate-gain-tables.py)

It is used by the cmake build process (see src/CMakeLists.txt) to generate
the C code used by libpg2sdr to hold the default gain tables.

The csv files are generated using lpcsdr_measurements/make-gain-curve.py, using
empirical data gathered by lpcsdr_measurements/gain-measurements.py

The current data uses measurements captured with:

  pg2sdr prototype (v2) s/n 381A2B63986061DC
  erasynth micro with 60dB of attenuators attached

