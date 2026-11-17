"""
Takes as input the output from vheader, which lists VDIF frame headers. From this a summary of a VDIF file is generated.

        $ ~/networking/vheader jive5ab_chain_out.vdif | python ../vdif_net/summarize_vdif_headers.py

        Sequence of 15997 valid frames
	        Thread 0
		        ref_epoch 0
		        s_from_ref_epoch 0 - 1 (no values missing in the range)
		        frame_num 0 - 7999 (no values missing in the range)

		        No frames out of order
		        Missing frames between: ref_epoch 0 s_from_ref_epoch 1 frame_num 1999 -> ref_epoch 0 s_from_ref_epoch 1 frame_num 2003

The vheader program is at https://github.com/hughbg/networking
"""

import sys


if len(sys.argv) != 1:
    print("Usage: summarize_vdif_headers.py")
    print("\tInput to stdin: output of vheader")
    exit(1)

# Load headers into memory so they can be scanned several times, not once.
class Header:
    def __init__(self):
        self.fields = {
            "valid": None,
            "ref_epoch": None,
            "s_from_ref_epoch": None,
            "frame_num": None,
            "thr_id": None
        }


    def store(self, name, value):
        assert value.isdigit(), f"{value} is not a positive integer"
        if name == "valid":
            assert value in [ "0", "1" ], f"validity {value} is not 0 or 1"
        v = int(value)
        self.fields[name] = v

    def __eq__(self, h):
        return self.fields["valid"] == h.fields["valid"] and \
            self.fields["ref_epoch"] == h.fields["ref_epoch"] and \
            self.fields["s_from_ref_epoch"] == h.fields["s_from_ref_epoch"] and \
            self.fields["frame_num"] == h.fields["frame_num"] and self.fields["thr_id"] == h.fields["thr_id"]

# Store all the headers but separate them into sequences of valid/invalid runs, based on input order
class Sequence:
    # valid or invalid sequence in order of reading
    def __init__(self, header):
        self.headers = [ header ]

    def store(self, header):
        assert header.fields["valid"] == self.headers[-1].fields["valid"]
        self.headers.append(header)

    def valid(self):
        # What is the validity?
        # Since all headers have the same validity just return the first
        return self.headers[0].fields["valid"]


class Sequences:
    def __init__(self):
        self.sequences = []

    def store(self, header):
        if len(self.sequences) == 0:
            # start from scratch
            self.sequences.append(Sequence(header))

        else:
            # if the incoming header validity matches the current sequence then add it
            if self.sequences[-1].valid() == header.fields["valid"]:
                self.sequences[-1].store(header)
            else:
                # Start a new sequence
                self.sequences.append(Sequence(header))

# Take a sequence of ordered frames and see if they have gaps
class SequenceChecker:
    # It's theoretically possible for the ref_epoch to tick over if a 6-month boundary is crossed.
    # However I don't _think_ that is done within a single stream.
    def __init__(self, max_frame_num):
        self.max_frame_num = max_frame_num
        self.ref_epoch = None
        self.s_from_ref_epoch = None
        self.frame_num = None
        self.gap = False
        
    def next_value(self, header):
        
        if self.ref_epoch is None:
            # first set
            self.ref_epoch = header.fields["ref_epoch"]
            self.s_from_ref_epoch = header.fields["s_from_ref_epoch"]
            self.frame_num = header.fields["frame_num"]
                
        else:
            expected_ref_epoch = self.ref_epoch
            expected_s_from_ref_epoch = self.s_from_ref_epoch
            expected_frame_num = self.frame_num+1
            
            if expected_frame_num > max_frame_num:
                # tick over the seconds
                expected_frame_num = 0
                expected_s_from_ref_epoch += 1
                

            if not ( expected_ref_epoch == header.fields["ref_epoch"] and \
                expected_s_from_ref_epoch == header.fields["s_from_ref_epoch"] and \
                expected_frame_num == header.fields["frame_num"] ):
                if ( self.ref_epoch == header.fields["ref_epoch"] and \
                    self.s_from_ref_epoch == header.fields["s_from_ref_epoch"] and \
                    self.frame_num == header.fields["frame_num"] ):

                    # The incoming frame is the same as the last one
                    print(f"\t\t Replicated: ref_epoch {self.ref_epoch} s_from_ref_epoch {self.s_from_ref_epoch} frame_num {self.frame_num}")
                else:
                    # It ain't the last one or the expected one
            
                    print(f"\t\t Missing frames between: ref_epoch {self.ref_epoch} s_from_ref_epoch {self.s_from_ref_epoch} frame_num {self.frame_num}" +\
                        f" -> ref_epoch {header.fields['ref_epoch']} s_from_ref_epoch {header.fields['s_from_ref_epoch']} frame_num {header.fields['frame_num']}")

                self.gap = True
                
            self.ref_epoch = header.fields["ref_epoch"]
            self.s_from_ref_epoch = header.fields["s_from_ref_epoch"]
            self.frame_num = header.fields["frame_num"]

valid_str = lambda i: "valid" if i==0 else "invalid"

### Load the headers - everything, and find global min/max of the ref_epoch, s_from_ref_epoch, frame_num, thread_ids.

sequences = Sequences()

line_num = 0
for line in sys.stdin:
    tokens = line[:-1].split()
    assert len(tokens)%2 == 0, "Must be an even number of tokens in a line (field name followed by value, repeated)"

    h = Header()
    for field in [ "valid", "s_from_ref_epoch", "ref_epoch", "frame_num", "thr_id" ]:
        try:
            index = tokens.index(field)
        except:
            print(f"Field {field} doesn't exist, line {line_num}")
            exit(1)

        if index == len(tokens)-1:
            print(f"No value found for {field}, line {line_num}")
            exit(1)
        h.store(field, tokens[index+1])

    sequences.store(h)

    line_num += 1
    
if line_num == 0:
    print("No headers received")
    exit(1)


    
### Now separate the sequences and then the different threads and analyse them ===================================

for sequence in sequences.sequences:

    # sequence is a class Sequence.

    print(f"Sequence of {len(sequence.headers)} {valid_str(sequence.headers[0].fields['valid'])} frames")

    # Find and split data out into threads
    threads = set([ h.fields["thr_id"] for h in sequence.headers ])

    for thr_id in threads:

        print("\tThread", thr_id)
        this_thread_headers = [ h for h in sequence.headers if h.fields["thr_id"] == thr_id ]

        # First, report on the ranges of header values
        for field in [ "ref_epoch", "s_from_ref_epoch", "frame_num" ]:
            vals = set([ h.fields[field] for h in this_thread_headers ])
            if len(vals) == 1:
                print("\t\t", field, min(vals))
            else:
                # We can get a range, but are all the values present in this range?
                should_have_this_many = max(vals)-min(vals)+1
                missing = should_have_this_many-len(vals)
                print("\t\t", field, min(vals), "-", max(vals), "(no" if missing==0 else "("+str(missing), "values missing in the range)")

            # save for later
            if field == "frame_num": max_frame_num = max(vals)

        headers_sorted = sorted(this_thread_headers, key = lambda h: (h.fields["ref_epoch"], h.fields["s_from_ref_epoch"], h.fields["frame_num"]))
        headers_in_order = True
        for i in range(len(this_thread_headers)):
            if not this_thread_headers[i] == headers_sorted[i]:
                print("\t\t", this_thread_headers[i].fields, "out of order")
                headers_in_order = False

        print()
        if headers_in_order:
            print("\t\t", "No frames out of order")

        sequence_checker = SequenceChecker(max_frame_num)
        for h in headers_sorted:
            sequence_checker.next_value(h)

        if not sequence_checker.gap:
            print("\t\t", "No missing frames found")
