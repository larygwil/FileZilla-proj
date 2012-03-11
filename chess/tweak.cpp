#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "random.hpp"
#include "tweak.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace {

static bool tweak_calc( position& p, color::type c, move& m, int& res, uint64_t move_time_limit, uint64_t time_remaining, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback& new_best_cb = default_new_best_move_callback )
{
	if( clock > 10 ) {
		calc_manager cmgr;
		return cmgr.calc( p, c, m, res, move_time_limit, time_remaining, clock, seen, last_mate, new_best_cb );
	}

	check_map check( p, c );

	move_info moves[200];
	move_info* pm = moves;
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
	pawn_hash_table.init( conf.pawn_hash_table_size );
	position p;

	init_board(p);

	unsigned int i = 1;
	color::type c = color::white;
	move m;
	int res = 0;

	seen_positions seen( get_zobrist_hash( p ) );

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
			seen.push_root( get_zobrist_hash( p ) );
		}
		else {
			seen.reset_root( get_zobrist_hash( p ) );
		}

		if( seen.depth() > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
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


class gene_t
{
public:
	gene_t( short* target, short min, short max, std::string const& name = "" )
		: target_(target)
		, min_(min)
		, max_(max)
		, name_(name)
	{
	}

	gene_t()
		: target_()
		, min_()
		, max_()
		, name_()
	{
	}

	short* target_;
	short min_;
	short max_;
	std::string name_;
};

struct reference_data {
	position p;
	color::type c;
	double target_eval;
	std::string fen;
};


std::vector<gene_t> genes;

void init_genes()
{
	genes.push_back( gene_t( &eval_values.material_values[pieces::pawn], 75, 110, "material_values[1]") );
	genes.push_back( gene_t( &eval_values.material_values[pieces::knight], 270, 460, "material_values[2]" ) );
	genes.push_back( gene_t( &eval_values.material_values[pieces::bishop], 270, 460, "material_values[3]" ) );
	genes.push_back( gene_t( &eval_values.material_values[pieces::rook], 470, 680, "material_values[4]" ) );
	genes.push_back( gene_t( &eval_values.material_values[pieces::queen], 870, 1500, "material_values[5]" ) );
	genes.push_back( gene_t( &eval_values.double_bishop, 0, 100, "double_bishop" ) );
	genes.push_back( gene_t( &eval_values.doubled_pawn[0], -35, 0, "doubled_pawn[0]" ) );
	genes.push_back( gene_t( &eval_values.passed_pawn[0], 0, 60, "passed_pawn[0]" ) );
	genes.push_back( gene_t( &eval_values.isolated_pawn[0], -30, 0, "isolated_pawn[0]" ) );
	genes.push_back( gene_t( &eval_values.connected_pawn[0], 0, 30, "connected_pawn[0]" ) );
	genes.push_back( gene_t( &eval_values.doubled_pawn[1], -35, 0, "doubled_pawn[1]" ) );
	genes.push_back( gene_t( &eval_values.passed_pawn[1], 0, 60, "passed_pawn[1]" ) );
	genes.push_back( gene_t( &eval_values.isolated_pawn[1], -30, 0, "isolated_pawn[1]" ) );
	genes.push_back( gene_t( &eval_values.connected_pawn[1], 0, 30, "connected_pawn[1]" ) );
	genes.push_back( gene_t( &eval_values.pawn_shield[0], 0, 100, "pawn_shield[0]" ) );
	genes.push_back( gene_t( &eval_values.pawn_shield[1], 0, 100, "pawn_shield[1]" ) );
	genes.push_back( gene_t( &eval_values.castled, 0, 50, "castled" ) );
	genes.push_back( gene_t( &eval_values.pin_absolute_bishop, 1, 30, "pin_absolute_bishop" ) );
	genes.push_back( gene_t( &eval_values.pin_absolute_rook, 1, 30, "pin_absolute_rook" ) );
	genes.push_back( gene_t( &eval_values.pin_absolute_queen, 1, 30, "pin_absolute_queen" ) );
	genes.push_back( gene_t( &eval_values.mobility_scale[0], 0, 500, "mobility_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.mobility_scale[1], 0, 500, "mobility_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.pin_scale[0], 0, 500, "pin_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.pin_scale[1], 0, 500, "pin_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.rooks_on_open_file_scale, 0, 500, "rooks_on_open_file_scale" ) );
	genes.push_back( gene_t( &eval_values.connected_rooks_scale[0], 0, 500, "connected_rooks_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.connected_rooks_scale[1], 0, 500, "connected_rooks_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.tropism_scale[0], 0, 500, "tropism_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.tropism_scale[1], 0, 500, "tropism_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.king_attack_by_piece[1], 1, 15, "king_attack_by_piece[1]") );
	genes.push_back( gene_t( &eval_values.king_attack_by_piece[2], 1, 15, "king_attack_by_piece[2]") );
	genes.push_back( gene_t( &eval_values.king_attack_by_piece[3], 1, 15, "king_attack_by_piece[3]") );
	genes.push_back( gene_t( &eval_values.king_attack_by_piece[4], 1, 15, "king_attack_by_piece[4]") );
	genes.push_back( gene_t( &eval_values.king_attack_by_piece[5], 1, 15, "king_attack_by_piece[5]") );
	genes.push_back( gene_t( &eval_values.king_check_by_piece[2], 1, 15, "king_check_by_piece[2]") );
	genes.push_back( gene_t( &eval_values.king_check_by_piece[3], 1, 15, "king_check_by_piece[3]") );
	genes.push_back( gene_t( &eval_values.king_check_by_piece[4], 1, 15, "king_check_by_piece[4]") );
	genes.push_back( gene_t( &eval_values.king_check_by_piece[5], 1, 15, "king_check_by_piece[5]") );
	genes.push_back( gene_t( &eval_values.king_melee_attack_by_rook, 1, 15, "king_melee_attack_by_rook") );
	genes.push_back( gene_t( &eval_values.king_melee_attack_by_queen, 1, 15, "king_melee_attack_by_queen") );
	genes.push_back( gene_t( &eval_values.king_attack_max, 100, 1000, "king_attack_max" ) );
	genes.push_back( gene_t( &eval_values.king_attack_rise, 1, 30, "king_attack_rise" ) );
	genes.push_back( gene_t( &eval_values.king_attack_exponent, 100, 300, "king_attack_exponent" ) );
	genes.push_back( gene_t( &eval_values.king_attack_offset, 0, 50, "king_attack_offset" ) );
	genes.push_back( gene_t( &eval_values.king_attack_scale[0], 0, 500, "king_attack_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.king_attack_scale[1], 0, 500, "king_attack_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.center_control_scale[0], 0, 500, "center_control_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.center_control_scale[1], 0, 500, "center_control_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.phase_transition_begin, 0, 3000, "phase_transition_begin" ) );
	genes.push_back( gene_t( &eval_values.phase_transition_duration, 1000, 4000, "phase_transition_duration" ) );
	genes.push_back( gene_t( &eval_values.material_imbalance_scale, 0, 500, "material_imbalance_scale" ) );
	genes.push_back( gene_t( &eval_values.rule_of_the_square, 0, 20, "rule_of_the_square" ) );
	genes.push_back( gene_t( &eval_values.passed_pawn_unhindered, 0, 20, "passed_pawn_unhindered" ) );
	genes.push_back( gene_t( &eval_values.unstoppable_pawn_scale[0], 0, 200, "unstoppable_pawn_scale[0]" ) );
	genes.push_back( gene_t( &eval_values.unstoppable_pawn_scale[1], 0, 200, "unstoppable_pawn_scale[1]" ) );
	genes.push_back( gene_t( &eval_values.hanging_piece[1], 1, 10, "hanging_piece[1]") );
	genes.push_back( gene_t( &eval_values.hanging_piece[2], 1, 10, "hanging_piece[2]") );
	genes.push_back( gene_t( &eval_values.hanging_piece[3], 1, 10, "hanging_piece[3]") );
	genes.push_back( gene_t( &eval_values.hanging_piece[4], 1, 10, "hanging_piece[4]") );
	genes.push_back( gene_t( &eval_values.hanging_piece[5], 1, 10, "hanging_piece[5]") );
	genes.push_back( gene_t( &eval_values.hanging_piece_scale[0], 0, 500, "hanging_piece_scale[0]") );
	genes.push_back( gene_t( &eval_values.hanging_piece_scale[1], 0, 500, "hanging_piece_scale[1]") );
	genes.push_back( gene_t( &eval_values.mobility_knight_min, -50, 0, "mobility_knight_min") );
	genes.push_back( gene_t( &eval_values.mobility_knight_max, 0, 50, "mobility_knight_max") );
	genes.push_back( gene_t( &eval_values.mobility_knight_rise, 1, 10, "mobility_knight_rise") );
	genes.push_back( gene_t( &eval_values.mobility_knight_offset, 0, 4, "mobility_knight_offset") );
	genes.push_back( gene_t( &eval_values.mobility_bishop_min, -50, 0, "mobility_bishop_min") );
	genes.push_back( gene_t( &eval_values.mobility_bishop_max, 0, 50, "mobility_bishop_max") );
	genes.push_back( gene_t( &eval_values.mobility_bishop_rise, 1, 10, "mobility_bishop_rise") );
	genes.push_back( gene_t( &eval_values.mobility_bishop_offset, 0, 7, "mobility_bishop_offset") );
	genes.push_back( gene_t( &eval_values.mobility_rook_min, -50, 0, "mobility_rook_min") );
	genes.push_back( gene_t( &eval_values.mobility_rook_max, 0, 50, "mobility_rook_max") );
	genes.push_back( gene_t( &eval_values.mobility_rook_rise, 1, 10, "mobility_rook_rise") );
	genes.push_back( gene_t( &eval_values.mobility_rook_offset, 0, 7, "mobility_rook_offset") );
	genes.push_back( gene_t( &eval_values.mobility_queen_min, -50, 0, "mobility_queen_min") );
	genes.push_back( gene_t( &eval_values.mobility_queen_max, 0, 50, "mobility_queen_max") );
	genes.push_back( gene_t( &eval_values.mobility_queen_rise, 1, 10, "mobility_queen_rise") );
	genes.push_back( gene_t( &eval_values.mobility_queen_offset, 0, 14, "mobility_queen_offset") );
}

struct individual
{
	individual()
		: fitness_()
		, max_diff_()
	{
		for( unsigned int i = 0; i < genes.size(); ++i ) {
			values_.push_back( *genes[i].target_ );
		}
	}

	individual( individual const& ref )
		: values_( ref.values_ )
		, fitness_( ref.fitness_ )
		, max_diff_( ref.max_diff_ )
	{
	}

	individual& operator=( individual const& ref )
	{
		values_ = ref.values_;
		fitness_ = ref.fitness_;
		max_diff_ = ref.max_diff_;

		return *this;
	}

	void randomize()
	{
		for( unsigned int i = 0; i < values_.size(); ++i ) {
			gene_t const& gene = genes[i];
			values_[i] = gene.min_ + (rand() % (gene.max_ - gene.min_ + 1));
		}
	}

	void apply()
	{
		for( unsigned int i = 0; i < values_.size(); ++i ) {
			*genes[i].target_ = values_[i];
		}
		eval_values.update_derived();
	}

	void calc_fitness( std::vector<reference_data>& data, bool verbose = false ) {
		apply();

		fitness_ = 0;
		max_diff_ = 0;
		for( std::size_t i = 0; i < data.size(); ++i ) {
			reference_data& ref = data[i];

			init_material( ref.p );
			short score = evaluate_full( ref.p, ref.c );

			if( verbose ) {
				if( std::abs(ref.target_eval - score) > 400  ) {
					std::cerr << "Ref: " << ref.target_eval << " Actual: " << score << " Fen: " << ref.fen << std::endl;
				}
			}

			double difference = std::abs( ref.target_eval - static_cast<double>(score) );

			fitness_ += difference;

			max_diff_ = (std::max)(max_diff_, static_cast<short>(difference));
		}
		fitness_ /= data.size();
	}

	bool operator<( individual const& rhs ) const {
		std::vector<short>::const_iterator it;
		std::vector<short>::const_iterator it2;
		for( it = values_.begin(), it2 = rhs.values_.begin(); it != values_.end(); ++it, ++it2 ) {
			if( *it < *it2 ) {
				return true;
			}
			if( *it2 < *it ) {
				return false;
			}
		}

		return false;
	}


	std::vector<short> values_;

	double fitness_;
	short max_diff_;
};


typedef std::vector<individual*> population;


void mutate( population& pop, std::vector<reference_data>& data )
{
	// Randomly mutates elements and adds the resulting mutants to the container
	std::size_t orig_count = pop.size();

	std::set<individual> seen;
	for( std::size_t i = 0; i < orig_count; ++i ) {
		seen.insert( *pop[i] );
	}
	
	for( int x = 0; x < 4; ++x ) {
		std::size_t loop_count = pop.size();
		for( std::size_t i = 0; i < loop_count; ++i ) {
			individual const& ref = *pop[i];
			{
				// Randomly mutate a field, decreasing it by one
				unsigned char fi = rand() % genes.size();
				gene_t const& gene = genes[fi];
				if( ref.values_[fi] > gene.min_ ) {
					individual* mut = new individual( ref );
					--mut->values_[fi];
					if( seen.insert( *mut ).second ) {
						pop.push_back( mut );
					}
					else {
						delete mut;
					}
				}
			}
			{
				// Randomly mutate a field, increasing it by one
				unsigned char fi = rand() % genes.size();
				gene_t const& gene = genes[fi];
				if( ref.values_[fi] < gene.max_ ) {
					individual* mut = new individual( ref );
					++mut->values_[fi];
					if( seen.insert( *mut ).second ) {
						pop.push_back( mut );
					}
					else {
						delete mut;
					}
				}
			}
			{
				// Randomly mutate a field using a random value
				unsigned char fi = rand() % genes.size();
				gene_t const& gene = genes[fi];

				short v = gene.min_ + (rand() % (gene.max_ - gene.min_ + 1));
				if( v != ref.values_[fi] ) {
					individual* mut = new individual( ref );
					mut->values_[fi] = v;
					if( seen.insert( *mut ).second ) {
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

void combine( population& pop,  std::vector<reference_data>& data )
{
	// Sex :)
	// Hopefully by coupling up, we end up with a child that's better than each of its parents.
	std::size_t orig_count = pop.size();
	if( orig_count < 1 ) {
		return;
	}

	std::set<individual> seen;
	for( std::size_t i = 0; i < orig_count; ++i ) {
		seen.insert( *pop[i] );
	}

	for( std::size_t r1 = 0; r1 < orig_count; ++r1 ) {
		std::size_t r2 = r1;
		while( r1 == r2 ) {
			r2 = (get_random_unsigned_long_long() + rand()) % orig_count;
		}

		individual const& i1 = *pop[r1];
		individual const& i2 = *pop[r2];

		individual* child( new individual(i1) );
		for( std::size_t i = 0; i < genes.size(); ++i ) {
			if( rand() % 2 ) {
				child->values_[i] = i2.values_[i];
			}
		}
		if( seen.insert( *child ).second ) {
			child->calc_fitness( data );
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

	std::size_t const max_size = 50;

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


void save_new_best( individual& best, std::vector<reference_data>& data )
{
	std::cout << std::endl;
	for( std::size_t i = 0; i < genes.size(); ++i ) {
		std::cerr << "\t" << std::left << std::setw(27) << genes[i].name_ << std::right << " = " << std::setw(5) << best.values_[i] << ";" << std::endl;
	}
	std::cout << "New best: " << best.fitness_ << " " << best.max_diff_ << std::endl;

	best.calc_fitness( data, true );
}


std::vector<reference_data> load_data()
{
	std::vector<reference_data> ret;

	std::ifstream in_fen("test/testpositions.txt");
	std::ifstream in_scores("test/data.txt");

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		std::string score_line;
		std::getline( in_scores, score_line );

		short score = 0;
		int count = 0;

		std::istringstream ss(score_line);
		short tmp;
		if( (ss >> tmp) ) {
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


void add_random( population & pop, std::vector<reference_data>& data ) {
	for( unsigned int i = 0; i < 10; ++i ) {
		individual* p = new individual;
		p->randomize();
		p->calc_fitness( data );
		pop.push_back( p );
	}
}


void tweak_evaluation()
{
	std::vector<reference_data> data = load_data();

	init_genes();

	double original_fitness = 999999;
	double best_fitness = original_fitness;

	population pop;
	pop.push_back( new individual() );
	//pop[0]->randomize();
	pop[0]->calc_fitness( data );
	
	while( true ) {
		std::cout << ".";
		std::flush( std::cout );
		srand ( time(NULL) );

		mutate( pop, data );
		add_random( pop, data );
		combine( pop, data );
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


