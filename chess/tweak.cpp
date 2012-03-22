#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
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

static bool tweak_calc( position& p, color::type c, move& m, int& res, duration const& move_time_limit, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback& new_best_cb = default_new_best_move_callback )
{
	if( clock > 10 ) {
		calc_manager cmgr;
		return cmgr.calc( p, c, m, res, move_time_limit, clock, seen, last_mate, new_best_cb );
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

	while( tweak_calc( p, c, m, res, duration(), i, seen, last_mate ) ) {
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
	short min_eval;
	short max_eval;
	double avg_eval;
	std::string fen;
};


std::vector<gene_t> genes;

void init_genes()
{
	genes.push_back( gene_t( &eval_values::mg_material_values[pieces::pawn], 70, 110, "mg_material_values[1]") );
	genes.push_back( gene_t( &eval_values::mg_material_values[pieces::knight], 270, 460, "mg_material_values[2]" ) );
	genes.push_back( gene_t( &eval_values::mg_material_values[pieces::bishop], 270, 460, "mg_material_values[3]" ) );
	genes.push_back( gene_t( &eval_values::mg_material_values[pieces::rook], 450, 680, "mg_material_values[4]" ) );
	genes.push_back( gene_t( &eval_values::mg_material_values[pieces::queen], 870, 1500, "mg_material_values[5]" ) );
	genes.push_back( gene_t( &eval_values::eg_material_values[pieces::pawn], 70, 110, "eg_material_values[1]") );
	genes.push_back( gene_t( &eval_values::eg_material_values[pieces::knight], 270, 460, "eg_material_values[2]" ) );
	genes.push_back( gene_t( &eval_values::eg_material_values[pieces::bishop], 270, 460, "eg_material_values[3]" ) );
	genes.push_back( gene_t( &eval_values::eg_material_values[pieces::rook], 450, 680, "eg_material_values[4]" ) );
	genes.push_back( gene_t( &eval_values::eg_material_values[pieces::queen], 870, 1500, "eg_material_values[5]" ) );
	genes.push_back( gene_t( &eval_values::double_bishop.mg(), 0, 100, "double_bishop.mg()" ) );
	genes.push_back( gene_t( &eval_values::double_bishop.eg(), 0, 100, "double_bishop.eg()" ) );
	genes.push_back( gene_t( &eval_values::doubled_pawn.mg(), -50, 0, "doubled_pawn.mg()" ) );
	genes.push_back( gene_t( &eval_values::doubled_pawn.eg(), -50, 0, "doubled_pawn.eg()" ) );
	genes.push_back( gene_t( &eval_values::passed_pawn.mg(), 0, 100, "passed_pawn.mg()" ) );
	genes.push_back( gene_t( &eval_values::passed_pawn.eg(), 0, 100, "passed_pawn.eg()" ) );
	genes.push_back( gene_t( &eval_values::isolated_pawn.mg(), -50, 0, "isolated_pawn.mg()" ) );
	genes.push_back( gene_t( &eval_values::isolated_pawn.eg(), -50, 0, "isolated_pawn.eg()" ) );
	genes.push_back( gene_t( &eval_values::connected_pawn.mg(), 0, 50, "connected_pawn.mg()" ) );
	genes.push_back( gene_t( &eval_values::connected_pawn.eg(), 0, 50, "connected_pawn.eg()" ) );
	genes.push_back( gene_t( &eval_values::candidate_passed_pawn.mg(), 0, 50, "candidate_passed_pawn.mg()" ) );
	genes.push_back( gene_t( &eval_values::candidate_passed_pawn.eg(), 0, 50, "candidate_passed_pawn.eg()" ) );
	genes.push_back( gene_t( &eval_values::pawn_shield.mg(), 0, 100, "pawn_shield.mg()" ) );
	genes.push_back( gene_t( &eval_values::pawn_shield.eg(), 0, 100, "pawn_shield.eg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[1].mg(), 0, 100, "absolute_pin[1].mg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[1].eg(), 0, 100, "absolute_pin[1].eg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[2].mg(), 0, 100, "absolute_pin[2].mg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[2].eg(), 0, 100, "absolute_pin[2].eg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[3].mg(), 0, 100, "absolute_pin[3].mg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[3].eg(), 0, 100, "absolute_pin[3].eg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[4].mg(), 0, 100, "absolute_pin[4].mg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[4].eg(), 0, 100, "absolute_pin[4].eg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[5].mg(), 0, 100, "absolute_pin[5].mg()" ) );
	genes.push_back( gene_t( &eval_values::absolute_pin[5].eg(), 0, 100, "absolute_pin[5].eg()" ) );
	genes.push_back( gene_t( &eval_values::rooks_on_open_file.mg(), 0, 100, "rooks_on_open_file.mg()" ) );
	genes.push_back( gene_t( &eval_values::rooks_on_open_file.eg(), 0, 100, "rooks_on_open_file.eg()" ) );
	genes.push_back( gene_t( &eval_values::rooks_on_half_open_file.mg(), 0, 100, "rooks_on_half_open_file.mg()" ) );
	genes.push_back( gene_t( &eval_values::rooks_on_half_open_file.eg(), 0, 100, "rooks_on_half_open_file.eg()" ) );
	genes.push_back( gene_t( &eval_values::connected_rooks.mg(), 0, 500, "connected_rooks.mg()" ) );
	genes.push_back( gene_t( &eval_values::connected_rooks.eg(), 0, 500, "connected_rooks.eg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[1].mg(), 0, 50, "tropism[1].mg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[1].eg(), 0, 50, "tropism[1].eg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[2].mg(), 0, 50, "tropism[2].mg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[2].eg(), 0, 50, "tropism[2].eg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[3].mg(), 0, 50, "tropism[3].mg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[3].eg(), 0, 50, "tropism[3].eg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[4].mg(), 0, 50, "tropism[4].mg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[4].eg(), 0, 50, "tropism[4].eg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[5].mg(), 0, 50, "tropism[5].mg()" ) );
	genes.push_back( gene_t( &eval_values::tropism[5].eg(), 0, 50, "tropism[5].eg()" ) );
	genes.push_back( gene_t( &eval_values::king_attack_by_piece[1], 0, 100, "king_attack_by_piece[1]") );
	genes.push_back( gene_t( &eval_values::king_attack_by_piece[2], 0, 100, "king_attack_by_piece[2]") );
	genes.push_back( gene_t( &eval_values::king_attack_by_piece[3], 0, 100, "king_attack_by_piece[3]") );
	genes.push_back( gene_t( &eval_values::king_attack_by_piece[4], 0, 100, "king_attack_by_piece[4]") );
	genes.push_back( gene_t( &eval_values::king_attack_by_piece[5], 0, 100, "king_attack_by_piece[5]") );
	genes.push_back( gene_t( &eval_values::king_check_by_piece[2], 0, 100, "king_check_by_piece[2]") );
	genes.push_back( gene_t( &eval_values::king_check_by_piece[3], 0, 100, "king_check_by_piece[3]") );
	genes.push_back( gene_t( &eval_values::king_check_by_piece[4], 0, 100, "king_check_by_piece[4]") );
	genes.push_back( gene_t( &eval_values::king_check_by_piece[5], 0, 100, "king_check_by_piece[5]") );
	genes.push_back( gene_t( &eval_values::king_melee_attack_by_rook, 0, 100, "king_melee_attack_by_rook") );
	genes.push_back( gene_t( &eval_values::king_melee_attack_by_queen, 0, 100, "king_melee_attack_by_queen") );
	genes.push_back( gene_t( &eval_values::king_attack_min[0], 0, 1000, "king_attack_min[0]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_max[0], 100, 1000, "king_attack_max[0]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_rise[0], 1, 30, "king_attack_rise[0]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_exponent[0], 100, 300, "king_attack_exponent[0]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_offset[0], 0, 100, "king_attack_offset[0]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_min[1], 0, 1000, "king_attack_min[1]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_max[1], 100, 1000, "king_attack_max[1]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_rise[1], 1, 50, "king_attack_rise[1]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_exponent[1], 100, 300, "king_attack_exponent[1]" ) );
	genes.push_back( gene_t( &eval_values::king_attack_offset[1], 0, 100, "king_attack_offset[1]" ) );
	genes.push_back( gene_t( &eval_values::center_control.mg(), 0, 500, "center_control.mg()" ) );
	genes.push_back( gene_t( &eval_values::center_control.eg(), 0, 500, "center_control.eg()" ) );
	genes.push_back( gene_t( &eval_values::material_imbalance.mg(), 0, 500, "material_imbalance.mg()" ) );
	genes.push_back( gene_t( &eval_values::material_imbalance.eg(), 0, 500, "material_imbalance.eg()" ) );
	genes.push_back( gene_t( &eval_values::rule_of_the_square.mg(), 0, 100, "rule_of_the_square.mg()" ) );
	genes.push_back( gene_t( &eval_values::rule_of_the_square.eg(), 0, 100, "rule_of_the_square.eg()" ) );
	genes.push_back( gene_t( &eval_values::passed_pawn_unhindered.mg(), 0, 100, "passed_pawn_unhindered.mg()" ) );
	genes.push_back( gene_t( &eval_values::passed_pawn_unhindered.eg(), 0, 100, "passed_pawn_unhindered.eg()" ) );
	genes.push_back( gene_t( &eval_values::hanging_piece[1].mg(), 0, 100, "hanging_piece[1].mg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[1].eg(), 0, 100, "hanging_piece[1].eg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[2].mg(), 0, 100, "hanging_piece[2].mg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[2].eg(), 0, 100, "hanging_piece[2].eg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[3].mg(), 0, 100, "hanging_piece[3].mg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[3].eg(), 0, 100, "hanging_piece[3].eg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[4].mg(), 0, 100, "hanging_piece[4].mg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[4].eg(), 0, 100, "hanging_piece[4].eg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[5].mg(), 0, 100, "hanging_piece[5].mg()") );
	genes.push_back( gene_t( &eval_values::hanging_piece[5].eg(), 0, 100, "hanging_piece[5].eg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_min.mg(), -100, 0, "mobility_knight_min.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_max.mg(), 0, 100, "mobility_knight_max.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_rise.mg(), 1, 50, "mobility_knight_rise.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_offset.mg(), 0, 4, "mobility_knight_offset.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_min.mg(), -100, 0, "mobility_bishop_min.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_max.mg(), 0, 100, "mobility_bishop_max.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_rise.mg(), 1, 50, "mobility_bishop_rise.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_offset.mg(), 0, 7, "mobility_bishop_offset.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_min.mg(), -100, 0, "mobility_rook_min.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_max.mg(), 0, 100, "mobility_rook_max.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_rise.mg(), 1, 50, "mobility_rook_rise.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_offset.mg(), 0, 7, "mobility_rook_offset.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_min.mg(), -100, 0, "mobility_queen_min.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_max.mg(), 0, 100, "mobility_queen_max.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_rise.mg(), 1, 50, "mobility_queen_rise.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_offset.mg(), 0, 14, "mobility_queen_offset.mg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_min.eg(), -100, 0, "mobility_knight_min.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_max.eg(), 0, 100, "mobility_knight_max.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_rise.eg(), 1, 50, "mobility_knight_rise.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_knight_offset.eg(), 0, 4, "mobility_knight_offset.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_min.eg(), -100, 0, "mobility_bishop_min.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_max.eg(), 0, 100, "mobility_bishop_max.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_rise.eg(), 1, 50, "mobility_bishop_rise.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_bishop_offset.eg(), 0, 7, "mobility_bishop_offset.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_min.eg(), -100, 0, "mobility_rook_min.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_max.eg(), 0, 50, "mobility_rook_max.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_rise.eg(), 1, 50, "mobility_rook_rise.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_rook_offset.eg(), 0, 7, "mobility_rook_offset.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_min.eg(), -100, 0, "mobility_queen_min.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_max.eg(), 0, 100, "mobility_queen_max.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_rise.eg(), 1, 50, "mobility_queen_rise.eg()") );
	genes.push_back( gene_t( &eval_values::mobility_queen_offset.eg(), 0, 14, "mobility_queen_offset.eg()") );
	genes.push_back( gene_t( &eval_values::side_to_move.mg(), 0, 100, "side_to_move.mg()") );
	genes.push_back( gene_t( &eval_values::side_to_move.eg(), 0, 100, "side_to_move.eg()") );
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
		eval_values::update_derived();
	}

	void calc_fitness( std::vector<reference_data>& data, bool verbose = false ) {
		apply();

		fitness_ = 0;
		max_diff_ = 0;
		for( std::size_t i = 0; i < data.size(); ++i ) {
			reference_data& ref = data[i];

			ref.p.update_derived();
			pawn_hash_table.clear( ref.p.pawn_hash );
			short score = evaluate_full( ref.p, ref.c );

			double difference = std::abs( ref.avg_eval - static_cast<double>(score) );
//			int difference = 0;
//			if( score < ref.min_eval ) {
//				difference = int(ref.avg_eval) - score;
//			}
//			else if( score > ref.max_eval ) {
//				difference = int(score) - ref.avg_eval;
//			}

			if( verbose ) {
				if( difference > 200  ) {
//					std::cerr << "Ref: min=" << ref.min_eval << " max=" << ref.max_eval << " avg=" << ref.avg_eval << " Actual: " << score << " Fen: " << ref.fen << std::endl;
				}
			}

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


void mutate( population& pop, std::set<individual>& seen, std::vector<reference_data>& data )
{
	// Randomly mutates elements and adds the resulting mutants to the container
	std::size_t orig_count = pop.size();

	for( int x = 0; x < 4; ++x ) {
		std::size_t loop_count = pop.size();
		for( std::size_t i = 0; i < loop_count; ++i ) {
			individual const& ref = *pop[i];
			{
				// Randomly mutate a field, decreasing it by one
				unsigned int fi = rand() % genes.size();
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
				unsigned int fi = rand() % genes.size();
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
				unsigned int fi = rand() % genes.size();
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

void combine( population& pop, std::set<individual>& seen, std::vector<reference_data>& data )
{
	// Sex :)
	// Hopefully by coupling up, we end up with a child that's better than each of its parents.
	std::size_t orig_count = pop.size();
	if( orig_count < 1 ) {
		return;
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

void select( population& pop, std::set<individual>& seen )
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

	seen.clear();
	for( population::const_iterator it = pop.begin(); it != pop.end(); ++it ) {
		seen.insert( **it );
	}
}


void save_new_best( individual& best, std::vector<reference_data>& data )
{
	std::cout << std::endl;
	for( std::size_t i = 0; i < genes.size(); ++i ) {
		std::cerr << "\t" << std::left << std::setw(30) << genes[i].name_ << std::right << " = " << std::setw(5) << best.values_[i] << ";" << std::endl;
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

		reference_data entry;
		entry.min_eval = result::win;
		entry.max_eval = result::loss;

		std::istringstream ss(score_line);
		double tmp;
		while( (ss >> tmp) ) {
			short v = static_cast<short>(tmp * 100);
			score += v;
			++count;
			entry.min_eval = (std::min)(entry.min_eval, v);
			entry.max_eval = (std::max)(entry.max_eval, v);
		}

		if( !count ) {
			continue;
		}

		score /= count;

		if( !in_scores ) {
			abort();
		}
		if( !parse_fen_noclock( fen, entry.p, entry.c ) ) {
			abort();
		}

		entry.avg_eval = score;
		entry.fen = fen;

		ret.push_back(entry);
	}

	std::cout << "Loaded " << ret.size() << " test positions." << std::endl;
	return ret;
}


void add_random( population & pop, std::set<individual>& seen, std::vector<reference_data>& data, unsigned int count = 10 ) {
	for( unsigned int i = 0; i < count; ++i ) {
		individual* p = new individual;
		p->randomize();
		p->calc_fitness( data );
		if( seen.insert( *p ).second ) {
			pop.push_back( p );
		}
		else {
			delete p;
		}
	}
}


void tweak_evaluation()
{
	pawn_hash_table.init( conf.pawn_hash_table_size );
	std::vector<reference_data> data = load_data();

	init_genes();

	double original_fitness = 999999;
	double best_fitness = original_fitness;

	population pop;
	std::set<individual> seen;

#if 0
	pop.push_back( new individual() );
	pop[0]->calc_fitness( data );
	seen.insert( *pop[0] );
#else
	add_random( pop, seen, data, 50 );
#endif

	while( true ) {
		std::cout << ".";
		std::flush( std::cout );
		srand ( time(NULL) );

		mutate( pop, seen, data );
		add_random( pop, seen, data );
		combine( pop, seen, data );
		sort( pop );
		select( pop, seen );

		if( pop.front()->fitness_ < best_fitness ) {
			save_new_best( *pop.front(), data );
			best_fitness = pop.front()->fitness_;
			for( std::size_t i = 1; i < pop.size(); ++i ) {
				delete pop[i];
			}
			pop.resize(1);
			seen.clear();
			seen.insert( *pop[0] );
		}
	}
}


