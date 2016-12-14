This is a TAGE predictor based on "A case for (partially) TAgged GEometric history length
branch prediction" by Seznec and Michaud. It is constrained to use no more than 64 KB memory.
It consists of 4 variable-length tables with varying tag sizes, with the hash based on the
1st history-folding technique described in “A PPM-like, tag-based branch predictor” by Michaud.
Each table entry has a single-bit usefulness counter used for allocation, and a single entry
is allocated on each misprediction on the first available table in increasing order of tag
length. A prediction and alt prediction are made using the first two available table entries for
a given branch, sorted in decreasing order of tag length. An alt counter is used to determine
whether the alt prediction should be used as the final prediction. If an entry is not allocated 
for the given branch in any of the 4 tables above, a simple base predictor with 3-bit saturating 
counters indexed by the PC address is used for the prediction.

The idea of the TAGE predictor is that a branch should be resolved by a predictor that is as
simple as possible...but no simpler. Those branches that need long history lengths in order
to be correctly resolved will benefit from having a corresponding predictor in the table with
the longest history length. Those that can be predicted by a simple 3-bit saturating counter
will never have allocations on the TAGE tables at all.

This branch predictor is meant to be run with the simulator available at
https://drive.google.com/open?id=0B5TPQrtyq9_kRmxmak1ESFF4cXc
. To install the simulator, run the following commands:

	tar -xzvf bpc6421AU15.tar.gz
	cd bpc6421AU15
	cd sim
	make ./predictor ../traces/SHORT-INT-1.cbp4.gz
	
After installing the simulator, overwrite the existing predictor.cc and predictor.h files
with the CC and H files in this repository. Run using either runall.pl or doit.sh in the 
scripts directory. You can view your results using the getdata.pl script.

This predictor achieves the following results with respect to misprediction rate:

LONG-SPEC2K6-00		1.884

LONG-SPEC2K6-01		7.442

LONG-SPEC2K6-02		0.628

LONG-SPEC2K6-03		1.479

LONG-SPEC2K6-04		9.818

LONG-SPEC2K6-05		5.538

LONG-SPEC2K6-06		0.719

LONG-SPEC2K6-07		11.772

LONG-SPEC2K6-08		0.833

LONG-SPEC2K6-09		3.964

SHORT-FP-1			1.752

SHORT-FP-2			0.679

SHORT-FP-3			0.018

SHORT-INT-1			0.410

SHORT-INT-2			6.541

SHORT-INT-3			9.370

SHORT-MM-1			7.974

SHORT-MM-2			10.448

SHORT-MM-3			0.095

SHORT-SERV-1		1.254

SHORT-SERV-2		1.216

SHORT-SERV-3		3.589

AMEAN				3.974