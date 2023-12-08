// ================================================================
// the Miller Shuffle Algorithm
//
// source: https://github.com/RondeSC/Miller_Shuffle_Algo
// license: Attribution-NonCommercial-ShareAlike
// Copyright 2022 Ronald R. Miller
// http://www.apache.org/licenses/LICENSE-2.0
//
// Update Aug 2022 added Miller Shuffle Algo-C (also a _lite version)
//   the Algo-C is by & large suitable to replace both algo-A and -B
// Update April 2023 added Miller Shuffle Algo-D 
//   The NEW Algo-D performs superiorly to earlier variants (~Fisher-Yates statistics).
//   Optimized so as to generate greater permutations of possible shuffles over time.
//   A -Max variant was also added, pushing (obsessively?) random-permutations to the limit.
// Update May 2023 added Miller Shuffle Algo-E and improved MS-lite.
//   Removed MSA-A and MSA-C as obsolite, having no use cases not better served by MS-lite or MSA-D
// Update June 2023: greatly increased the potential number by unique shuffle permutations for MSA_d & MSA_e
//    Also improved the randomness and permutations of MS_lite.
// Update Aug 2023: updated MillerShuffleAlgo-E to rely on 'Chinese remainder theorem' to ensure unique randomizing factors
//


// --------------------------------------------------------------
// the Miller Shuffle Algorithms
// produces a shuffled Index given a base Index, a shuffle ID "seed" and the length of the list being
// indexed. For each inx: 0 to listSize-1, unique indexes are returned in a pseudo "random" order.
// Utilizes minimum resources. 
// As such the Miller Shuffle algorithm is the better choice for a playlist shuffle.
//
// The 'shuffleID' is an unsigned 32bit value and can be selected by utilizing a PRNG.
// Each time you want another pseudo random index from a current shuffle (incrementing 'inx')
// you must be sure to pass in the "shuffleID" for that shuffle.
// Note that you can exceed the listSize with the input 'inx' value and get very good results,
// as the code effectively uses a secondary shuffle by way of using a 'working' modified value of the input shuffle ID.


// --------------------------------------------------------------
// Miller Shuffle Algorithm D variant
//    aka:   MillerShuffleAlgo_d
unsigned int MillerShuffle(unsigned int inx, unsigned int shuffleID, unsigned int listSize) {
  unsigned int si, r1, r2, r3, r4, rx, rx2;
  const unsigned int p1=24317, p2=32141, p3=63629;  // for shuffling 60,000+ indexes

  shuffleID+=131*(inx/listSize);  // have inx overflow effect the mix
  si=(inx+shuffleID)%listSize;    // cut the deck

  r1=shuffleID%p1+42;   // randomizing factors crafted empirically (by automated trial and error)
  r2=((shuffleID*0x89)^r1)%p2;
  r3=(r1+r2+p3)%listSize;
  r4=r1^r2^r3;
  rx = (shuffleID/listSize) % listSize + 1;
  rx2 = ((shuffleID/listSize/listSize)) % listSize + 1;

  // perform conditional multi-faceted mathematical spin-mixing (on avg 2 1/3 shuffle ops done + 2 simple Xors)
  if (si%3==0) si=(((unsigned long)(si/3)*p1+r1) % ((listSize+2)/3)) *3; // spin multiples of 3 
  if (si%2==0) si=(((unsigned long)(si/2)*p2+r2) % ((listSize+1)/2)) *2; // spin multiples of 2 
  if (si<listSize/2) si=(si*p3+r4) % (listSize/2);

  if ((si^rx) < listSize)   si ^= rx;			// flip some bits with Xor
  si = ((unsigned long)si*p3 + r3) % listSize;  // relatively prime gears turning operation
  if ((si^rx2) < listSize)  si ^= rx2;
	
  return(si);  // return 'Shuffled' index
}

