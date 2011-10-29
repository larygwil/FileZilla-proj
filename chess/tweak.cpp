#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "tweak.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern const int PAWN_HASH_TABLE_SIZE;

namespace {

static bool tweak_calc( position& p, color::type c, move& m, int& res, unsigned long long move_time_limit, unsigned long long time_remaining, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback& new_best_cb = default_new_best_move_callback )
{
	if( clock > 10 ) {
		return calc( p, c, m, res, move_time_limit, time_remaining, clock, seen, last_mate, new_best_cb );
	}

	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int current_evaluation = evaluate_fast( p, c );
	calculate_moves( p, c, pm, check );

	if( moves == pm ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cerr << "BLACK WINS" << std::endl;
				res = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				res = result::win;
			}
			return false;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			res = result::draw;
			return false;
		}
	}

	m = (moves + ((rand() + get_random_unsigned_long_long()) % (pm - moves)))->m;

	return true;
}

static void generate_test_positions_impl()
{
	conf.max_moves = 20 + get_random_unsigned_long_long() % 70;

	transposition_table.clear_data();
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );
	unsigned long long start = get_time();
	position p;

	init_board(p);

	unsigned int i = 1;
	color::type c = color::white;
	move m;
	int res = 0;

	seen_positions seen;
	seen.root_position = 0;
	seen.pos[0] = get_zobrist_hash( p );

	short last_mate = 0;

	while( tweak_calc( p, c, m, res, 0, 0, i, seen, last_mate ) ) {
		if( res > result::win_threshold ) {
			last_mate = res;
		}

		if( !validate_move( p, m, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		bool reset_seen = false;
		if( m.piece == pieces::pawn || m.captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, m, c );

		c = static_cast<color::type>(1-c);

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p );
		}

		if( seen.root_position > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}

		++i;

		if( conf.max_moves && i >= conf.max_moves ) {
			std::ofstream out("test/testpositions.txt", std::ofstream::app|std::ofstream::out );

			std::string fen = position_to_fen_noclock( p, c );
			out << fen << std::endl;

			break;
		}
	}

	unsigned long long stop = get_time();
}
}

void generate_test_positions()
{
	transposition_table.init( conf.memory );
	
	conf.depth = 6;
	
	while( true ) {
		generate_test_positions_impl();
	}
}


struct gene_t
{
	gene_t( short* value, short min, short max )
		: value_(value)
		, min_(min)
		, max_(max)
	{
	}

	gene_t()
		: value_()
		, min_()
		, max_()
	{
	}

	short* value_;
	short min_;
	short max_;
};

int const gene_count = 34;

struct reference_data {
	position p;
	color::type c;
	double target_eval;
	std::string fen;
};


struct individual
{
	individual()
		: fitness_()
	{
		genes_[ 0] = gene_t( &eval_data_.material_values[pieces::pawn], 90, 110 );
		genes_[ 1] = gene_t( &eval_data_.material_values[pieces::knight], 270, 330 );
		genes_[ 2] = gene_t( &eval_data_.material_values[pieces::bishop], 270, 330 );
		genes_[ 3] = gene_t( &eval_data_.material_values[pieces::rook], 470, 530 );
		genes_[ 4] = gene_t( &eval_data_.material_values[pieces::queen], 870, 930 );
		genes_[ 5] = gene_t( &eval_data_.double_bishop, 0, 35 );
		genes_[ 6] = gene_t( &eval_data_.doubled_pawn[0], -30, 0 );
		genes_[ 7] = gene_t( &eval_data_.passed_pawn[0], 0, 50 );
		genes_[ 8] = gene_t( &eval_data_.isolated_pawn[0], -30, 0 );
		genes_[ 9] = gene_t( &eval_data_.connected_pawn[0], 0, 30 );
		genes_[10] = gene_t( &eval_data_.doubled_pawn[1], -30, 0 );
		genes_[11] = gene_t( &eval_data_.passed_pawn[1], 0, 50 );
		genes_[12] = gene_t( &eval_data_.isolated_pawn[1], -30, 0 );
		genes_[13] = gene_t( &eval_data_.connected_pawn[1], 0, 30 );
		genes_[14] = gene_t( &eval_data_.pawn_shield[0], 0, 10 );
		genes_[15] = gene_t( &eval_data_.pawn_shield[1], 0, 10 );
		genes_[16] = gene_t( &eval_data_.castled, 0, 50 );
		genes_[17] = gene_t( &eval_data_.pin_absolute_bishop, 0, 50 );
		genes_[18] = gene_t( &eval_data_.pin_absolute_rook, 0, 50 );
		genes_[19] = gene_t( &eval_data_.pin_absolute_queen, 0, 50 );
		genes_[20] = gene_t( &eval_data_.mobility_multiplicator, 0, 10 );
		genes_[21] = gene_t( &eval_data_.mobility_divisor, 1, 10 );
		genes_[22] = gene_t( &eval_data_.pin_multiplicator, 0, 10 );
		genes_[23] = gene_t( &eval_data_.pin_divisor, 1, 10 );
		genes_[24] = gene_t( &eval_data_.rooks_on_open_file_multiplicator, 0, 10 );
		genes_[25] = gene_t( &eval_data_.rooks_on_open_file_divisor, 1, 10 );
		genes_[26] = gene_t( &eval_data_.tropism_multiplicator, 0, 10 );
		genes_[27] = gene_t( &eval_data_.tropism_divisor, 1, 10 );
		genes_[28] = gene_t( &eval_data_.king_attack_multiplicator, 0, 10 );
		genes_[29] = gene_t( &eval_data_.king_attack_divisor, 1, 10 );
		genes_[30] = gene_t( &eval_data_.center_control_multiplicator, 0, 10 );
		genes_[31] = gene_t( &eval_data_.center_control_divisor, 1, 10 );
		genes_[32] = gene_t( &eval_data_.phase_transition_begin, 0, 1000 );
		genes_[33] = gene_t( &eval_data_.phase_transition_duration, 1, 2000 );
	}

	individual( individual const& ref )
		: eval_data_( ref.eval_data_ )
		, fitness_()
	{
		// I U+2665 C-style pointer arithmetic
		intptr_t diff = reinterpret_cast<const char*>(&ref.eval_data_) - reinterpret_cast<const char*>(&eval_data_);
		for( int i = 0; i < gene_count; ++i ) {
			genes_[i] = ref.genes_[i];
			genes_[i].value_ = reinterpret_cast<short*>(reinterpret_cast<char*>(genes_[i].value_) - diff);
		}
	}

	individual& operator=( individual const& ref )
	{
		eval_data_ = ref.eval_data_;

		intptr_t diff = reinterpret_cast<const char*>(&ref.eval_data_) - reinterpret_cast<const char*>(&eval_data_);
		for( int i = 0; i < gene_count; ++i ) {
			genes_[i] = ref.genes_[i];
			genes_[i].value_ = reinterpret_cast<short*>(reinterpret_cast<char*>(genes_[i].value_) - diff);
		}

		return *this;
	}

	void calc_fitness( std::vector<reference_data> const& data, bool verbose = false ) {
		eval_data_.update_derived();
		eval_values = eval_data_;

		fitness_ = 0;
		for( std::size_t i = 0; i < data.size(); ++i ) {
			reference_data const& ref = data[i];

			short score = evaluate_full( ref.p, ref.c );

			if( verbose ) {
				std::cerr << "Ref: " << ref.target_eval << " Actual: " << score << " Fen: " << ref.fen << std::endl;
			}
			double difference = std::abs( ref.target_eval - static_cast<double>(score) );

			fitness_ += difference;
		}
		fitness_ /= data.size();
	}

	gene_t genes_[gene_count];
	eval_values_t eval_data_;

	double fitness_;
};


bool operator<( eval_values_t const& lhs, eval_values_t const& rhs ) {
	return memcmp(&lhs, &rhs, sizeof(eval_values_t)) < 0;
}


typedef std::vector<individual*> population;


void mutate( population& pop, std::vector<reference_data> const& data )
{
	// Randomly mutates elements and adds the resulting mutants to the container
	std::size_t orig_count = pop.size();

	std::set<eval_values_t> seen;
	for( std::size_t i = 0; i < orig_count; ++i ) {
		seen.insert( pop[i]->eval_data_ );
	}
	
	for( int x = 0; x < 4; ++x ) {
		std::size_t loop_count = pop.size();
		for( std::size_t i = 0; i < loop_count; ++i ) {
			individual const& ref = *pop[i];
			{
				// Randomly mutate a field, decreasing it by one
				unsigned char fi = rand() % gene_count;
				gene_t const& gene = ref.genes_[fi];
				if( *gene.value_ > gene.min_ ) {
					individual* mut = new individual( ref );
					--(*mut->genes_[fi].value_);
					if( seen.insert( mut->eval_data_ ).second ) {
						pop.push_back( mut );
					}
					else {
						delete mut;
					}
				}
			}
			{
				// Randomly mutate a field, increasing it by one
				unsigned char fi = rand() % gene_count;
				gene_t const& gene = ref.genes_[fi];
				if( *gene.value_ < gene.max_ ) {
					individual* mut = new individual( ref );
					++(*mut->genes_[fi].value_);
					if( seen.insert( mut->eval_data_ ).second ) {
						pop.push_back( mut );
					}
					else {
						delete mut;
					}
				}
			}
			{
				// Randomly mutate a field using a random value
				unsigned char fi = rand() % gene_count;
				gene_t const& gene = ref.genes_[fi];

				short v = gene.min_ + (rand() % (gene.max_ - gene.min_ + 1));
				if( v != *gene.value_ ) {
					individual* mut = new individual( ref );
					*mut->genes_[fi].value_ = v;
					if( seen.insert( mut->eval_data_ ).second ) {
						pop.push_back( mut );
					}
					else {
						delete mut;
					}
				}
			}
		}
	}

	for( std::size_t i = orig_count; i < pop.size(); ++i ) {
		// Calculate fitness of new items
		pop[i]->calc_fitness(data);
	}
}

void combine( population& pop )
{
	// Sex :)
	// Hopefully by coupling up, we end up with a child that's better than each of its parents.
	std::size_t orig_count = pop.size();
	if( orig_count < 1 ) {
		return;
	}

	std::set<eval_values_t> seen;
	for( std::size_t i = 0; i < orig_count; ++i ) {
		seen.insert( pop[i]->eval_data_ );
	}

	for( int r1 = 0; r1 < orig_count; ++r1 ) {
		int r2 = r1;
		while( r1 == r2 ) {
			r2 = (get_random_unsigned_long_long() + rand()) % orig_count;
		}

		individual const& i1 = *pop[r1];
		individual const& i2 = *pop[r2];

		individual* child( new individual(i1) );
		for( int i = 0; i < gene_count; ++i ) {
			if( rand() % 2 ) {
				child->genes_[i].value_ = i2.genes_[i].value_;
			}
		}
		if( seen.insert( child->eval_data_ ).second ) {
			pop.push_back( child );
		}
		else {
			delete child;
		}
	}
}

namespace {
struct FitnessSort {
	bool operator()( individual const* lhs, individual const* rhs ) const {
		return lhs->fitness_ < rhs->fitness_;
	}
} fitnessSort;
}


void sort( population& pop )
{
	std::sort( pop.begin(), pop.end(), fitnessSort );
}

void select( population& pop )
{
	// Cull population. Always keep the first n best 
	// individuals (elitism).
	std::size_t const n = 10;

	std::size_t const max_size = 100;

	if( pop.size() <= max_size ) {
		return;
	}

	population out;
	out.reserve( max_size );
	for( std::size_t i = 0; i < (std::min)(n, pop.size()); ++i ) {
		out.push_back( pop[i] );
	}

	std::size_t limit = pop.size() - n;
	while( out.size() < max_size ) {
		std::size_t r = (get_random_unsigned_long_long() + rand()) % limit;

		out.push_back( pop[n + r] );
		pop[n + r] = pop[n + limit - 1];
		--limit;
	}

	for( std::size_t i = n; i < n + limit; ++i ) {
		delete pop[i];
	}

	std::swap( out, pop );
}


void save_new_best( individual& best, std::vector<reference_data> const& data )
{
	std::cout << std::endl;
	std::cout << "New best: " << best.fitness_ << std::endl;
	for( std::size_t i = 0; i < gene_count; ++i ) {
		std::cerr << " " << *best.genes_[i].value_ << std::endl;
	}

	//best.calc_fitness( data, true );
}


std::vector<reference_data> load_data()
{
	std::vector<reference_data> ret;

	std::ifstream in_fen("test/testpositions.txt");
	std::ifstream in_scores("test/data.txt");

	{
		std::string dummy;
		std::getline( in_fen, dummy );
		std::getline( in_scores, dummy );
	}

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		std::string score_line;
		std::getline( in_scores, score_line );

		short score = 0;
		int count = 0;

		std::istringstream ss(score_line);
		short tmp;
		while( (ss >> tmp) ) {
			score += tmp;
			++count;
		}

		score /= count;

		if( !in_scores ) {
			abort();
		}
		reference_data entry;
		if( !parse_fen_noclock( fen, entry.p, entry.c ) ) {
			abort();
		}

		entry.target_eval = score;
		entry.fen = fen;

		ret.push_back(entry);
	}

	std::cout << "Loaded " << ret.size() << " test positions." << std::endl;
	return ret;
}


void tweak_evaluation()
{
	std::vector<reference_data> const data = load_data();

	double original_fitness = 999999;
	double best_fitness = original_fitness;

	population pop;
	pop.push_back( new individual() );
	pop[0]->calc_fitness( data );
	
	while( true ) {
		std::cout << ".";
		std::flush( std::cout );
		srand ( time(NULL) );
		mutate( pop, data );
		combine( pop );
		sort( pop );
		select( pop );

		if( pop.front()->fitness_ < best_fitness ) {
			save_new_best( *pop.front(), data );
			best_fitness = pop.front()->fitness_;
			for( std::size_t i = 1; i < pop.size(); ++i ) {
				delete pop[i];
			}
			pop.resize(1);
		}
	}
}


