#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "sam.h"
#include "render.h"
#include "SamTabs.h"

char input[256]; //tab39445
//standard sam sound
unsigned char speed = 72;
unsigned char pitch = 64;
unsigned char mouth = 128;
unsigned char throat = 128;
int singmode = 0;

extern int debug;

unsigned char mem39;
unsigned char mem44;
unsigned char mem47;
unsigned char mem49;
unsigned char mem50;
unsigned char mem51;
unsigned char mem53;
unsigned char mem59=0;

unsigned char X;

unsigned char stress[256]; //numbers from 0 to 8
unsigned char phonemeLength[256]; //tab40160
unsigned char phonemeindex[256];

unsigned char phonemeIndexOutput[60]; //tab47296
unsigned char stressOutput[60]; //tab47365
unsigned char phonemeLengthOutput[60]; //tab47416




// contains the final soundbuffer
int bufferpos=0;
char *buffer = NULL;


void SetInput(char *_input)
{
	int i, l;
	l = strlen(_input);
	if (l > 254) l = 254;
	for(i=0; i<l; i++)
		input[i] = _input[i];
	input[l] = 0;
}

void SetSpeed(unsigned char _speed) {speed = _speed;};
void SetPitch(unsigned char _pitch) {pitch = _pitch;};
void SetMouth(unsigned char _mouth) {mouth = _mouth;};
void SetThroat(unsigned char _throat) {throat = _throat;};
void EnableSingmode() {singmode = 1;};
char* GetBuffer(){return buffer;};
int GetBufferLength(){return bufferpos;};

void Init();
int Parser1();
void Parser2();
int SAMMain();
void CopyStress();
void SetPhonemeLength();
void AdjustLengths();
void Code41240();
void Insert(unsigned char position, unsigned char mem60, unsigned char mem59, unsigned char mem58);
void InsertBreath();
void PrepareOutput();
void SetMouthThroat(unsigned char mouth, unsigned char throat);

// 168=pitches 
// 169=frequency1
// 170=frequency2
// 171=frequency3
// 172=amplitude1
// 173=amplitude2
// 174=amplitude3


void Init()
{
	int i;
	SetMouthThroat( mouth, throat);

	bufferpos = 0;
	// TODO, check for free the memory, 10 seconds of output should be more than enough
	buffer = malloc(22050*10); 

	/*
	freq2data = &mem[45136];
	freq1data = &mem[45056];
	freq3data = &mem[45216];
	*/
	//pitches = &mem[43008];
	/*
	frequency1 = &mem[43264];
	frequency2 = &mem[43520];
	frequency3 = &mem[43776];
	*/
	/*
	amplitude1 = &mem[44032];
	amplitude2 = &mem[44288];
	amplitude3 = &mem[44544];
	*/
	//phoneme = &mem[39904];
	/*
	ampl1data = &mem[45296];
	ampl2data = &mem[45376];
	ampl3data = &mem[45456];
	*/

	for(i=0; i<256; i++)
	{
		stress[i] = 0;
		phonemeLength[i] = 0;
	}
	
	for(i=0; i<60; i++)
	{
		phonemeIndexOutput[i] = 0;
		stressOutput[i] = 0;
		phonemeLengthOutput[i] = 0;
	}
	phonemeindex[255] = 255; //to prevent buffer overflow // ML : changed from 32 to 255 to stop freezing with long inputs

}

int SAMMain() {
	Init();
	phonemeindex[255] = 32; //to prevent buffer overflow

	if (!Parser1()) return 0;
	if (debug) PrintPhonemes(phonemeindex, phonemeLength, stress);
	Parser2();
	CopyStress();
	SetPhonemeLength();
	AdjustLengths();
	Code41240();
	do {
		if (phonemeindex[X] > 80) {
			phonemeindex[X] = 255;
			break; // error: delete all behind it
		}
	} while (++X != 0);
	InsertBreath();

	if (debug) PrintPhonemes(phonemeindex, phonemeLength, stress);

	PrepareOutput();
	return 1;
}

void PrepareOutput() {
	unsigned char X = 0; // Position in source
	unsigned char Y = 0; // Position in output

	while(1) {
		unsigned char A = phonemeindex[X];

        phonemeIndexOutput[Y] = A;

		if (A == 255) { // End of input
			Render();
			return;
		}

		if (A == 254) {
			++X;
			phonemeIndexOutput[Y] = 255;
			Render();
			Y = 0;
			continue;
		}

		if (A == 0) {
			++X;
			continue;
		}

		phonemeIndexOutput[Y]  = A;
		phonemeLengthOutput[Y] = phonemeLength[X];
		stressOutput[Y]        = stress[X];
		++X;
		++Y;
	}
}


void InsertBreath() {
	unsigned char mem54 = 255;
	unsigned char mem55 = 0;
	unsigned char index; //variable Y

	unsigned char pos = 0;

	while((index = phonemeindex[pos]) != 255) {
		mem55 += phonemeLength[pos];

		if (mem55 < 232) {
			if (index != 254) { // ML : Prevents an index out of bounds problem
                if((flags2[index]&1) != 0) {
					mem55 = 0;
					Insert(pos+1, 254, mem59, 0);
					pos += 2;
					continue;
				}
			}
			if (index == 0) mem54 = pos;
			++pos;
			continue;
		}

		phonemeindex[mem54]  = 31;   // 'Q*' glottal stop
		phonemeLength[mem54] = 4;
		stress[mem54] = 0;
		mem55 = 0;
		Insert(mem54+1, 254, mem59, 0);
		pos = mem54+2;
	}
}


// Iterates through the phoneme buffer, copying the stress value from
// the following phoneme under the following circumstance:
       
//     1. The current phoneme is voiced, excluding plosives and fricatives
//     2. The following phoneme is voiced, excluding plosives and fricatives, and
//     3. The following phoneme is stressed
//
//  In those cases, the stress value+1 from the following phoneme is copied.
//
// For example, the word LOITER is represented as LOY5TER, with as stress
// of 5 on the dipthong OY. This routine will copy the stress value of 6 (5+1)
// to the L that precedes it.



void CopyStress() {
    // loop thought all the phonemes to be output
	unsigned char pos=0; //mem66
    unsigned char Y;
	while((Y = phonemeindex[pos]) != 255) {
		// if CONSONANT_FLAG set, skip - only vowels get stress
		if ((flags[Y] & 64) == 0) {pos++; continue;}
		Y = phonemeindex[pos+1];
		if (Y == 255) //prevent buffer overflow
		{
			pos++; continue;
		}
		// if the following phoneme is a vowel, skip
        if ((flags[Y] & 128) == 0)  {pos++; continue;}

        // get the stress value at the next position
		Y = stress[pos+1];

		// if next phoneme is not stressed, skip
		if (Y == 0)  {pos++; continue;}
		// if next phoneme is not a VOWEL OR ER, skip
		if ((Y & 128) != 0)  {pos++; continue;}

		// copy stress from prior phoneme to this one
		stress[pos] = Y+1;
		
		++pos;
	}
}

void Insert(unsigned char position/*var57*/, unsigned char mem60, unsigned char mem59, unsigned char mem58)
{
	int i;
	for(i=253; i >= position; i--) // ML : always keep last safe-guarding 255	
	{
		phonemeindex[i+1] = phonemeindex[i];
		phonemeLength[i+1] = phonemeLength[i];
		stress[i+1] = stress[i];
	}

	phonemeindex[position] = mem60;
	phonemeLength[position] = mem59;
	stress[position] = mem58;
	return;
}

// The input[] buffer contains a string of phonemes and stress markers along
// the lines of:
//
//     DHAX KAET IHZ AH5GLIY. <0x9B>
//
// The byte 0x9B marks the end of the buffer. Some phonemes are 2 bytes 
// long, such as "DH" and "AX". Others are 1 byte long, such as "T" and "Z". 
// There are also stress markers, such as "5" and ".".
//
// The first character of the phonemes are stored in the table signInputTable1[].
// The second character of the phonemes are stored in the table signInputTable2[].
// The stress characters are arranged in low to high stress order in stressInputTable[].
// 
// The following process is used to parse the input[] buffer:
// 
// Repeat until the <0x9B> character is reached:
//
//        First, a search is made for a 2 character match for phonemes that do not
//        end with the '*' (wildcard) character. On a match, the index of the phoneme 
//        is added to phonemeIndex[] and the buffer position is advanced 2 bytes.
//
//        If this fails, a search is made for a 1 character match against all
//        phoneme names ending with a '*' (wildcard). If this succeeds, the 
//        phoneme is added to phonemeIndex[] and the buffer position is advanced
//        1 byte.
// 
//        If this fails, search for a 1 character match in the stressInputTable[].
//        If this succeeds, the stress value is placed in the last stress[] table
//        at the same index of the last added phoneme, and the buffer position is
//        advanced by 1 byte.
//
//        If this fails, return a 0.
//
// On success:
//
//    1. phonemeIndex[] will contain the index of all the phonemes.
//    2. The last index in phonemeIndex[] will be 255.
//    3. stress[] will contain the stress value for each phoneme

// input[] holds the string of phonemes, each two bytes wide
// signInputTable1[] holds the first character of each phoneme
// signInputTable2[] holds te second character of each phoneme
// phonemeIndex[] holds the indexes of the phonemes after parsing input[]
//
// The parser scans through the input[], finding the names of the phonemes
// by searching signInputTable1[] and signInputTable2[]. On a match, it
// copies the index of the phoneme into the phonemeIndexTable[].
//
// The character <0x9B> marks the end of text in input[]. When it is reached,
// the index 255 is placed at the end of the phonemeIndexTable[], and the
// function returns with a 1 indicating success.
int Parser1()
{
	int i;
	unsigned char sign1;
	unsigned char sign2;
	unsigned char position = 0;
	unsigned char X = 0;
	unsigned char A = 0;
	unsigned char Y = 0;

	// CLEAR THE STRESS TABLE
	for(i=0; i<256; i++) stress[i] = 0;

    // THIS CODE MATCHES THE PHONEME LETTERS TO THE TABLE
	while(1) {
        // GET THE FIRST CHARACTER FROM THE PHONEME BUFFER
		sign1 = input[X];
		// TEST FOR 155 (�) END OF LINE MARKER
		if (sign1 == 155) {
			phonemeindex[position] = 255;      //mark endpoint
			return 1;  // REACHED END OF PHONEMES, SO EXIT
		}

		// GET THE NEXT CHARACTER FROM THE BUFFER
		sign2 = input[++X];

		// NOW sign1 = FIRST CHARACTER OF PHONEME, AND sign2 = SECOND CHARACTER OF PHONEME

       // TRY TO MATCH PHONEMES ON TWO TWO-CHARACTER NAME
       // IGNORE PHONEMES IN TABLE ENDING WITH WILDCARDS
		Y = 0; // SET INDEX TO 0
pos41095:
         // GET FIRST CHARACTER AT POSITION Y IN signInputTable
         // --> should change name to PhonemeNameTable1
		A = signInputTable1[Y];
		
		if (A == sign1) {
           // GET THE CHARACTER FROM THE PhonemeSecondLetterTable
			A = signInputTable2[Y];
			// NOT A SPECIAL AND MATCHES SECOND CHARACTER?
			if ((A != '*') && (A == sign2))
			{
               // STORE THE INDEX OF THE PHONEME INTO THE phomeneIndexTable
				phonemeindex[position] = Y;

				// ADVANCE THE POINTER TO THE phonemeIndexTable
				++position;
				// ADVANCE THE POINTER TO THE phonemeInputBuffer
				++X;
				// CONTINUE PARSING
				continue;
			}
		}
		
		// NO MATCH, TRY TO MATCH ON FIRST CHARACTER TO WILDCARD NAMES (ENDING WITH '*')

		// ADVANCE TO THE NEXT POSITION. IF NOT END OF TABLE, CONTINUE
		if (++Y != 81) goto pos41095;

// REACHED END OF TABLE WITHOUT AN EXACT (2 CHARACTER) MATCH.
// THIS TIME, SEARCH FOR A 1 CHARACTER MATCH AGAINST THE WILDCARDS

// RESET THE INDEX TO POINT TO THE START OF THE PHONEME NAME TABLE
		Y = 0;
pos41134:
// DOES THE PHONEME IN THE TABLE END WITH '*'?
		if (signInputTable2[Y] == '*')
		{
// DOES THE FIRST CHARACTER MATCH THE FIRST LETTER OF THE PHONEME
			if (signInputTable1[Y] == sign1)
			{
                // SAVE THE POSITION AND MOVE AHEAD
				phonemeindex[position] = Y;
				
				// ADVANCE THE POINTER
				position++;
				
				// CONTINUE THROUGH THE LOOP
				continue;
			}
		}
		Y++;
		if (Y != 81) goto pos41134; //81 is size of PHONEME NAME table

// FAILED TO MATCH WITH A WILDCARD. ASSUME THIS IS A STRESS
// CHARACTER. SEARCH THROUGH THE STRESS TABLE

        // SET INDEX TO POSITION 8 (END OF STRESS TABLE)
		Y = 8;
		
       // WALK BACK THROUGH TABLE LOOKING FOR A MATCH
		while( (sign1 != stressInputTable[Y]) && (Y>0))
		{
  // DECREMENT INDEX
			Y--;
		}

        // REACHED THE END OF THE SEARCH WITHOUT BREAKING OUT OF LOOP?
		if (Y == 0)
		{
			//mem[39444] = X;
			//41181: JSR 42043 //Error
           // FAILED TO MATCH ANYTHING, RETURN 0 ON FAILURE
			return 0;
		}
// SET THE STRESS FOR THE PRIOR PHONEME
		stress[position-1] = Y;
	} //while
}


//change phonemelength depedendent on stress
void SetPhonemeLength() {
	unsigned char A;
	int position = 0;
	while(phonemeindex[position] != 255 )
	{
		A = stress[position];
		if ((A == 0) || ((A&128) != 0)) {
			phonemeLength[position] = phonemeLengthTable[phonemeindex[position]];
		} else {
			phonemeLength[position] = phonemeStressedLengthTable[phonemeindex[position]];
		}
		position++;
	}
}

void Code41240() {
	unsigned char pos=0;

	while(phonemeindex[pos] != 255) {
		unsigned char index; //register AC
		unsigned char X = pos;
		index = phonemeindex[pos];
		if ((flags[index]&2) == 0) {
			pos++;
			continue;
		} else if ((flags[index]&1) == 0) {
			Insert(pos+1, index+1, phonemeLengthTable[index+1], stress[pos]);
			Insert(pos+2, index+2, phonemeLengthTable[index+2], stress[pos]);
			pos += 3;
			continue;
		}

        unsigned char A;
        while(!(A = phonemeindex[++X]));

		if (A != 255) {
			if (flags[A] & 8)    { ++pos; continue;}
			if ((A == 36) || (A == 37)) {++pos; continue;} // '/H' '/X'
		}

		Insert(pos+1, index+1, phonemeLengthTable[index+1], stress[pos]);
		Insert(pos+2, index+2, phonemeLengthTable[index+2], stress[pos]);
		pos += 3;
	}
}


void ChangeRule(unsigned char position, unsigned char rule,unsigned char mem60, unsigned char mem59, unsigned char stress, const char * descr)
{
    if (debug) printf("RULE: %s\n",descr);
    phonemeindex[position] = rule;
    Insert(position+1, mem60, mem59, stress);
}


// Rewrites the phonemes using the following rules:
//
//       <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
//       <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
//       UL -> AX L
//       UM -> AX M
//       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
//       T R -> CH R
//       D R -> J R
//       <VOWEL> R -> <VOWEL> RX
//       <VOWEL> L -> <VOWEL> LX
//       G S -> G Z
//       K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
//       G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
//       S P -> S B
//       S T -> S D
//       S K -> S G
//       S KX -> S GX
//       <ALVEOLAR> UW -> <ALVEOLAR> UX
//       CH -> CH CH' (CH requires two phonemes to represent it)
//       J -> J J' (J requires two phonemes to represent it)
//       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
//       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>


void Parser2() {
	if (debug) printf("Parser2\n");
	unsigned char pos = 0; //mem66;

    // Loop through phonemes
	while(1) {
		X = pos;
		unsigned char p = phonemeindex[pos];
        unsigned char A = p;

        // DEBUG: Print phoneme and index
		if (debug && p != 255) printf("%d: %c%c\n", X, signInputTable1[p], signInputTable2[p]);

		if (p == 0) { // Is phoneme pause?
			++pos;
			continue;
		}

		if (p == 255) return;

        unsigned char pf = flags[p];

        // RULE: 
        //       <DIPTHONG ENDING WITH WX> -> <DIPTHONG ENDING WITH WX> WX
        //       <DIPTHONG NOT ENDING WITH WX> -> <DIPTHONG NOT ENDING WITH WX> YX
        // Example: OIL, COW

        // Check for DIPTHONG
		if ((pf & 16) != 0) { // Not a dipthong.
            // If ends with IY, use YX, else use WX
            A = (pf & 32) ? 21 : 20; // 'WX' = 20 'YX' = 21
            
            // Insert at WX or YX following, copying the stress
            
            if (debug) if (A==20) printf("RULE: insert WX following dipthong NOT ending in IY sound\n");
            if (debug) if (A==21) printf("RULE: insert YX following dipthong ending in IY sound\n");
            Insert(pos+1, A, mem59, stress[pos]);
            X = pos;

            goto pos41749;
        }

        if (p == 78 || p == 79 || p == 80) {
            if (p == 78) ChangeRule(X, 13, 24, mem59, stress[X],"UL -> AX L");       // Example: MEDDLE
            else if (p == 79) ChangeRule(X, 13, 27, mem59, stress[X], "UM -> AX M"); // Example: ASTRONOMY
            else if (p == 80) ChangeRule(X, 13, 28, mem59, stress[X], "UN -> AX N"); // Example: FUNCTION
            ++pos;
            continue;
        }

        // RULE:
        //       <STRESSED VOWEL> <SILENCE> <STRESSED VOWEL> -> <STRESSED VOWEL> <SILENCE> Q <VOWEL>
        // EXAMPLE: AWAY EIGHT

        if ((pf & 128) && stress[X]) { // VOWEL && stressed
            A = phonemeindex[++X];
            if (A == 0) { // If following phoneme is a pause, get next
                unsigned char Y = phonemeindex[++X];
                // Check for end of buffer flag
                if (Y == 255) { //buffer overflow
                    // ??? Not sure about these flags
                    A = 65&128;
                } else {
                    // And VOWEL flag to current phoneme's flags
                    A = flags[Y] & 128;
                }
                
                // If following phonemes is not a pause
                if (A != 0) {
                    // If the following phoneme is not stressed
                    A = stress[X];
                    if (A != 0) {
                        // Insert a glottal stop and move forward
                        if (debug) printf("RULE: Insert glottal stop between two stressed vowels with space between them\n");
                        Insert(X, 31, mem59, 0); // 31 = 'Q'
                        pos++;
                        continue;
                    }
                }
            }
        }

        // Get current position and phoneme
		X = pos;
		A = p;
        
        unsigned char prior = phonemeindex[pos-1];

		if (p == 23) { // 'R'
            // RULES FOR PHONEMES BEFORE R
            // Look at prior phoneme
            X--;
            A = prior;

            // Example: TRACK
            if (prior == 69) { // 'T'
                drule("T R -> CH R");
                phonemeindex[pos-1] = 42;
                goto pos41779;
            }
            
            // Example: DRY
            if (prior == 57) { // 'D'
                drule("D R -> J R");
                phonemeindex[pos-1] = 44;
                goto pos41788;
            }
            
            // Example: ART
            if (flags[prior] & 128) {
                drule("<VOWEL> R -> <VOWEL> RX");
                phonemeindex[pos] = 18;  // 'RX'
            }
            
            ++pos;
            continue;
        }

        if (p == 24 || p == 32 || A == 60) {
            // Example: ALL
            if (p == 24 && (flags[prior] & 128)) { // 'L' + prior has VOWEL flag set
                drule("<VOWEL> L -> <VOWEL> LX");
                phonemeindex[X] = 19;     // 'LX'
            }

            // G S -> G Z
            // Can't get to fire -
            //       1. The G -> GX rule intervenes
            //       2. Reciter already replaces GS -> GZ
            if (prior == 60 && p == 32) { // 'G' 'S'
                drule("G S -> G Z");
                phonemeindex[pos] = 38;    // 'Z'
            }

            if (p == 60) { // 'G'
                // G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
                // Example: GO

                unsigned char index = phonemeindex[pos+1];
            
                // If dipthong ending with YX, move continue processing next phoneme
                if ((index != 255) && ((flags[index] & 32) == 0)) {
                    // replace G with GX and continue processing next phoneme
                    if (debug) printf("RULE: G <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> GX <VOWEL OR DIPTHONG NOT ENDING WITH IY>\n");
                    phonemeindex[pos] = 63; // 'GX'
                }
            }

            pos++;
            continue;
		}
		

        //             K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>
        // Example: COW

		if (A == 72) {  // 'K'
			unsigned char Y = phonemeindex[pos+1];
            // If at end, replace current phoneme with KX
			if (Y == 255) phonemeindex[pos] = 75; // ML : prevents an index out of bounds problem
			else {
                // VOWELS AND DIPTHONGS ENDING WITH IY SOUND flag set?
				A = flags[Y] & 32;
				if (debug) if (A==0) printf("RULE: K <VOWEL OR DIPTHONG NOT ENDING WITH IY> -> KX <VOWEL OR DIPTHONG NOT ENDING WITH IY>\n");
                // Replace with KX
				if (A == 0) phonemeindex[pos] = 75;  // 'KX'
			}
		}

        // RULE:
        //      S P -> S B
        //      S T -> S D
        //      S K -> S G
        //      S KX -> S GX
        // Examples: SPY, STY, SKY, SCOWL

		unsigned char Y = phonemeindex[pos];
        // Replace with softer version?
		if ((flags[Y] & 1) && (prior == 32)) { // 'S'
            // Replace with softer version
            if (debug) printf("RULE: S* %c%c -> S* %c%c\n", signInputTable1[Y], signInputTable2[Y],signInputTable1[Y-12], signInputTable2[Y-12]);
            phonemeindex[pos] = Y-12;
            pos++;
            continue;
        } 

        if ((flags[Y] & 1)) {
            A = Y;
            goto pos41812;
        }

    pos41749:
        // RULE:
        //      <ALVEOLAR> UW -> <ALVEOLAR> UX
        //
        // Example: NEW, DEW, SUE, ZOO, THOO, TOO
		A = phonemeindex[X];
		if (A == 53) { // 'UW'
            // ALVEOLAR flag set?
			A = flags2[phonemeindex[X-1]] & 4;
			if (A) {
                if (debug) printf("RULE: <ALVEOLAR> UW -> <ALVEOLAR> UX\n");
                phonemeindex[X] = 16;
            }
			++pos;
			continue;
		}

    pos41779:
        // RULE:
        //       CH -> CH CH' (CH requires two phonemes to represent it)
        // Example: CHEW
		if (A == 42) {   // 'CH'
			if (debug) printf("CH -> CH CH+1\n");
			Insert(X+1, 43, mem59, stress[X]);
			pos++;
			continue;
		}

    pos41788:
        // RULE:
        //       J -> J J' (J requires two phonemes to represent it)
        // Example: JAY
		if (A == 44) { // 'J'
			if (debug) printf("J -> J J+1\n");
			Insert(X+1, 45, mem59, stress[X]);
			pos++;
			continue;
		}

        // Jump here to continue 
    pos41812:
        // RULE: Soften T following vowel
        // NOTE: This rule fails for cases such as "ODD"
        //       <UNSTRESSED VOWEL> T <PAUSE> -> <UNSTRESSED VOWEL> DX <PAUSE>
        //       <UNSTRESSED VOWEL> D <PAUSE>  -> <UNSTRESSED VOWEL> DX <PAUSE>
        // Example: PARTY, TARDY

		if (A == 69 || A == 57) { // 'T', 'D'

            // If prior phoneme is not a vowel, continue processing phonemes
            if (flags[phonemeindex[X-1]] & 128) {
                if ((A = phonemeindex[++X])) {
                    if (flags[A] & 128) { // pause
                        // FIXME: How does a pause get stressed?
                        if (stress[X] == 0) { // unstressed
                            // Set phonemes to DX
                            if (debug) printf("RULE: Soften T or D following vowel or ER and preceding a pause -> DX\n");
                            phonemeindex[pos] = 30;       // 'DX'
                        }
                    }
                } else {
                    A = phonemeindex[X+1];
                    if (A == 255) //prevent buffer overflow
                        A = 65 & 128;
                    else
                        // Is next phoneme a vowel or ER?
                        if (flags[A] & 128) {
                            if (debug) printf("RULE: Soften T or D following vowel or ER and preceding a pause -> DX\n");
                            if (A != 0) phonemeindex[pos] = 30;  // 'DX'
                        }
                }
            }
        }
        pos++;
	} // while
}

void drule(const char * str) {
    if (debug) printf("RULE: %s\n",str);
}

void drule_pre(const char *descr, unsigned char X) {
    if (debug) printf("RULE: %s\n", descr);
    if (debug) printf("PRE\n");
    if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]],  phonemeLength[X]);
}

void drule_post(unsigned char X) {
    if (debug) printf("POST\n");
    if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
}

// Applies various rules that adjust the lengths of phonemes
//
//         Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5
//         <VOWEL> <RX | LX> <CONSONANT> - decrease <VOWEL> length by 1
//         <VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th
//         <VOWEL> <UNVOICED CONSONANT> - increase vowel by 1/2 + 1
//         <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6
//         <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1
//         <LIQUID CONSONANT> <DIPTHONG> - decrease by 2

void AdjustLengths() {
    // LENGTHEN VOWELS PRECEDING PUNCTUATION
    //
    // Search for punctuation. If found, back up to the first vowel, then
    // process all phonemes between there and up to (but not including) the punctuation.
    // If any phoneme is found that is a either a fricative or voiced, the duration is
    // increased by (length * 1.5) + 1

    // loop index
	unsigned char X = 0;
	unsigned char index;

    // iterate through the phoneme list
	unsigned char loopIndex=0;
	while((index = phonemeindex[X]) != 255) {
		// not punctuation?
		if((flags2[index] & 1) == 0) { // skip
			++X;
			continue;
		}
		
		// hold index
		loopIndex = X;

        while (--X && !(flags[phonemeindex[X]] & 128)); // back up while not a vowel
        if (X == 0) break;

		do {
            // test for vowel
			index = phonemeindex[X];

			//if (index != 255)//inserted to prevent access overrun
			// test for fricative/unvoiced or not voiced
			if(((flags2[index] & 32) == 0) || ((flags[index] & 4) != 0))     //nochmal �berpr�fen
			{
				unsigned char A = phonemeLength[X];

				// change phoneme length to (length * 1.5) + 1
				A = (A >> 1) + A + 1;
                if (debug) printf("RULE: Lengthen <FRICATIVE> or <VOICED> between <VOWEL> and <PUNCTUATION> by 1.5\n");
                if (debug) printf("PRE\n");
                if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);

				phonemeLength[X] = A;
                
                if (debug) printf("POST\n");
                if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
			}
            // keep moving forward
			X++;
		} while (X != loopIndex);
		X++;
	}  // while

    // Similar to the above routine, but shorten vowels under some circumstances

    // Loop throught all phonemes
	loopIndex = 0;

	while(1) {
        // get a phoneme
		X = loopIndex;
		index = phonemeindex[X];
		
		// exit routine at end token
		if (index == 255) return;

		// vowel?
		unsigned char A = flags[index] & 128;
        unsigned char mem56;
		if (A != 0) {
			index = phonemeindex[++X];
			// get flags
			if (index == 255) 
                mem56 = 65; // use if end marker
			else
                mem56 = flags[index];

            // not a consonant
			if ((flags[index] & 64) == 0) {
                // RX or LX?
				if ((index == 18) || (index == 19)) { // 'RX' & 'LX'
					index = phonemeindex[++X];

					// next phoneme a consonant?
					if ((flags[index] & 64) != 0) {
                        drule_pre("<VOWEL> <RX | LX> <CONSONANT> - decrease length of vowel by 1\n", loopIndex);
    					phonemeLength[loopIndex]--;
                        drule_post(loopIndex);
                    }
				}
				// move ahead
				loopIndex++;
				continue;
			}
			
			
			// Got here if not <VOWEL>

            // not voiced
			if ((mem56 & 4) == 0) {
                 // Unvoiced 
                 // *, .*, ?*, ,*, -*, DX, S*, SH, F*, TH, /H, /X, CH, P*, T*, K*, KX
                 
                // not an unvoiced plosive?
				if((mem56 & 1) == 0) {
                    // move ahead
                    loopIndex++; 
                    continue;
                }

                // RULE: <VOWEL> <UNVOICED PLOSIVE>
                // <VOWEL> <P*, T*, K*, KX>

                // move back
				--X;

                drule_pre("<VOWEL> <UNVOICED PLOSIVE> - decrease vowel by 1/8th",X);
				mem56 = phonemeLength[X] >> 3;
				phonemeLength[X] -= mem56;
                drule_post(X);

				loopIndex++;
				continue;
			}

            // RULE: <VOWEL> <VOICED CONSONANT>
            // <VOWEL> <WH, R*, L*, W*, Y*, M*, N*, NX, DX, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX>

            drule_pre("<VOWEL> <VOICED CONSONANT> - increase vowel by 1/2 + 1\n",X-1);
            // decrease length
			A = phonemeLength[X-1];
			phonemeLength[X-1] = (A >> 2) + A + 1;     // 5/4*A + 1
            drule_post(X);

			loopIndex++;
			continue;
		}

        // WH, R*, L*, W*, Y*, M*, N*, NX, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX
        
        // RULE: <NASAL> <STOP CONSONANT>
        //       Set punctuation length to 6
        //       Set stop consonant length to 5
           
        // nasal?
        if((flags2[index] & 8) != 0) {
            // M*, N*, NX, 

            index = phonemeindex[++X];

            // end of buffer?
            if (index == 255)
               A = 65&2;  //prevent buffer overflow
            else
                A = flags[index] & 2; // check for stop consonant

            // is next phoneme a stop consonant?
            if (A != 0) {
               // B*, D*, G*, GX, P*, T*, K*, KX

                if (debug) printf("RULE: <NASAL> <STOP CONSONANT> - set nasal = 5, consonant = 6\n");
                if (debug) printf("POST\n");
                if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
                if (debug) printf("phoneme %d (%c%c) length %d\n", X-1, signInputTable1[phonemeindex[X-1]], signInputTable2[phonemeindex[X-1]], phonemeLength[X-1]);

                phonemeLength[X]   = 6; // set stop consonant length to 6
                phonemeLength[X-1] = 5; // set nasal length to 5

                if (debug) printf("POST\n");
                if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
                if (debug) printf("phoneme %d (%c%c) length %d\n", X-1, signInputTable1[phonemeindex[X-1]], signInputTable2[phonemeindex[X-1]], phonemeLength[X-1]);
            }
            loopIndex++;
            continue;
        }

        // WH, R*, L*, W*, Y*, Q*, Z*, ZH, V*, DH, J*, B*, D*, G*, GX

        // RULE: <VOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
        //       Shorten both to (length/2 + 1)

        // (voiced) stop consonant?
        if((flags[index] & 2) != 0) {
            // B*, D*, G*, GX

            // move past silence
            while ((index = phonemeindex[++X]) == 0);

            // check for end of buffer
            if (index == 255) //buffer overflow
            {
                // ignore, overflow code
                if ((65 & 2) == 0) {loopIndex++; continue;}
            } else if ((flags[index] & 2) == 0) {
                // if another stop consonant, move ahead
                loopIndex++;
                continue;
            }

            // RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT>
            if (debug) printf("RULE: <UNVOICED STOP CONSONANT> {optional silence} <STOP CONSONANT> - shorten both to 1/2 + 1\n");
            if (debug) printf("PRE\n");
            if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
            if (debug) printf("phoneme %d (%c%c) length %d\n", X-1, signInputTable1[phonemeindex[X-1]], signInputTable2[phonemeindex[X-1]], phonemeLength[X-1]);
            // X gets overwritten, so hold prior X value for debug statement
            int debugX = X;
            // shorten the prior phoneme length to (length/2 + 1)
            phonemeLength[X] = (phonemeLength[X] >> 1) + 1;
            X = loopIndex;

            // also shorten this phoneme length to (length/2 +1)
            phonemeLength[loopIndex] = (phonemeLength[loopIndex] >> 1) + 1;

            if (debug) printf("POST\n");
            if (debug) printf("phoneme %d (%c%c) length %d\n", debugX, signInputTable1[phonemeindex[debugX]], signInputTable2[phonemeindex[debugX]], phonemeLength[debugX]);
            if (debug) printf("phoneme %d (%c%c) length %d\n", debugX-1, signInputTable1[phonemeindex[debugX-1]], signInputTable2[phonemeindex[debugX-1]], phonemeLength[debugX-1]);

            loopIndex++;
            continue;
        }
        // WH, R*, L*, W*, Y*, Q*, Z*, ZH, V*, DH, J*, **, 

        // RULE: <VOICED NON-VOWEL> <DIPTHONG>
        //       Decrease <DIPTHONG> by 2

        // liquic consonant?
        if ((flags2[index] & 16) != 0) {
            // R*, L*, W*, Y*
                           
            // get the prior phoneme
            index = phonemeindex[X-1];


            // FIXME: The debug code here breaks the rule.
            // prior phoneme a stop consonant>
            if((flags[index] & 2) != 0) 
                // Rule: <LIQUID CONSONANT> <DIPTHONG>

if (debug) printf("RULE: <LIQUID CONSONANT> <DIPTHONG> - decrease by 2\n");
if (debug) printf("PRE\n");
if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
             
             // decrease the phoneme length by 2 frames (20 ms)
             phonemeLength[X] -= 2;

if (debug) printf("POST\n");
if (debug) printf("phoneme %d (%c%c) length %d\n", X, signInputTable1[phonemeindex[X]], signInputTable2[phonemeindex[X]], phonemeLength[X]);
         }

         // move to next phoneme
         loopIndex++;
         continue;
    }
}
