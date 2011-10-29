#ifndef __TWEAK_H__
#define __TWEAK_H__

/*
 * Genetic algorithm to tweak the evaluation routine.
 *
 * The general idea is the following:
 * 1) Generate a few hundred positions.
 * 2) For each test position, gather static evaluation score from
 *    several different engines.
 * 3) Calculate average score for each position and a confidence
 *    value (weighting factor)
 * 4) Evolve own evaluation function by tweaking the various
 *    constants using a genetic algorithm.
 *    The closer the own evaluation is to the weighted averages,
 *    the higher the fitness.
 */

void generate_test_positions();
void tweak_evaluation();

#endif //__TWEAK_H__
