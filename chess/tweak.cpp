#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "endgame.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "random.hpp"
#include "string.hpp"
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
#include <memory>

int const THRESHOLD = 300;

namespace {

static calc_result tweak_calc( position& p, color::type c, duration const& move_time_limit, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback_base& new_best_cb = default_new_best_move_callback )
{
	if( clock > 10 ) {
		calc_manager cmgr;
		return cmgr.calc( p, c, move_time_limit, move_time_limit, clock, seen, last_mate, new_best_cb );
	}

	check_map check( p, c );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, pm, check );

	calc_result result;
	if( moves == pm ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cerr << "BLACK WINS" << std::endl;
				result.forecast = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				result.forecast = result::win;
			}
			return result;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			result.forecast = result::draw;
			return result;
		}
	}

	result.best_move = (moves + ((rand() + get_random_unsigned_long_long()) % (pm - moves)))->m;

	return result;
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

	seen_positions seen( get_zobrist_hash( p ) );

	short last_mate = 0;

	calc_result result;
	while( !(result = tweak_calc( p, c, duration(), i, seen, last_mate ) ).best_move.empty() ) {
		if( result.forecast > result::win_threshold ) {
			last_mate = result.forecast;
		}

		if( !validate_move( p, result.best_move, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		bool reset_seen = false;
		if( result.best_move.piece == pieces::pawn || result.best_move.captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, result.best_move, c );

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
	gene_t( short* target, short min, short max )
		: target_(target)
		, min_(min)
		, max_(max)
	{
	}

	gene_t()
		: target_()
		, min_()
		, max_()
	{
	}

	short value() const { return *target_; }

	short* target_;
	short min_;
	short max_;
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


class tweak_base
{
public:
	tweak_base( std::string const& name )
		: name_(name)
	{
	}

	virtual std::string to_string() const = 0;

	std::string name_;
	std::vector<int> genes_;
};


class tweak_score : public tweak_base
{
public:
	tweak_score( score& target, int min, int max, std::string const& name )
		: tweak_base(name)
	{
		genes_.push_back(genes.size());
		genes.push_back( gene_t( &target.mg(), min, max ) );
		genes_.push_back(genes.size());
		genes.push_back( gene_t( &target.eg(), min, max ) );
	}

	virtual std::string to_string() const {
		std::stringstream ss;
		ss << std::left << std::setw(31) << name_ << " = score( ";
		ss << genes[genes_[0]].value();
		ss << ", ";
		ss << genes[genes_[1]].value();
		ss << " );" << std::endl;

		return ss.str();
	}
};


class tweak_short : public tweak_base
{
public:
	tweak_short( short& target, int min, int max, std::string const& name )
		: tweak_base(name)
	{
		genes_.push_back(genes.size());
		genes.push_back( gene_t( &target, min, max ) );
	}

	virtual std::string to_string() const {
		std::stringstream ss;
		ss << std::left << std::setw(31) << name_ << " = ";
		ss << genes[genes_[0]].value();
		ss << ";" << std::endl;

		return ss.str();
	}
};


std::vector<std::shared_ptr<tweak_base> > tweaks;

#define MAKE_GENE( name, min, max ) { \
	make_gene( eval_values:: name, min, max, #name ); \
}

#define MAKE_GENES( name, min, max, count, offset ) { \
	make_genes( eval_values:: name, min, max, #name, count, offset ); \
}

void make_gene( short& s, short min, short max, std::string const& name )
{
	tweaks.push_back( std::shared_ptr<tweak_base>( new tweak_short( s, min, max, name ) ) );
}

void make_gene( score& s, short min, short max, std::string const& name )
{
	tweaks.push_back( std::shared_ptr<tweak_base>( new tweak_score( s, min, max, name ) ) );
}

void make_genes( score* s, short min, short max, std::string const& name, int count, int offset = 0 )
{
	for( int i = offset; i < count + offset; ++i ) {
		tweaks.push_back( std::shared_ptr<tweak_base>( new tweak_score( s[i], min, max, name + "[" + to_string(i) + "]" ) ) );
	}
}

void make_genes( short* s, short min, short max, std::string const& name, int count, int offset = 0 )
{
	for( int i = offset; i < count + offset; ++i ) {
		tweaks.push_back( std::shared_ptr<tweak_base>( new tweak_short( s[i], min, max, name + "[" + to_string(i) + "]" ) ) );
	}
}

void init_genes()
{
	MAKE_GENE( material_values[pieces::pawn], 45, 110 );
	MAKE_GENE( material_values[pieces::knight], 260, 460 );
	MAKE_GENE( material_values[pieces::bishop], 260, 460 );
	MAKE_GENE( material_values[pieces::rook], 400, 680 );
	MAKE_GENE( material_values[pieces::queen], 870, 1500 );
	MAKE_GENE( double_bishop, 0, 100 );

	MAKE_GENE( passed_pawn_advance_power, 100, 210 );
	MAKE_GENES( passed_pawn_king_distance, 0, 10, 2, 0 );

	MAKE_GENE( passed_pawn_base[0], 0, 100 );
	MAKE_GENES( passed_pawn_base, 0, 20, 3, 1 );

	MAKE_GENE( doubled_pawn_base[0][0], 0, 50 );
	MAKE_GENES( doubled_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( doubled_pawn_base[1][0], 0, 50 );
	MAKE_GENES( doubled_pawn_base[1], 0, 20, 3, 1 );


	MAKE_GENE( backward_pawn_base[0][0], 0, 50 );
	MAKE_GENES( backward_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( backward_pawn_base[1][0], 0, 50 );
	MAKE_GENES( backward_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( isolated_pawn_base[0][0], 0, 50 );
	MAKE_GENES( isolated_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( isolated_pawn_base[1][0], 0, 50 );
	MAKE_GENES( isolated_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( connected_pawn_base[0][0], 0, 50 );
	MAKE_GENES( connected_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( connected_pawn_base[1][0], 0, 50 );
	MAKE_GENES( connected_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( candidate_passed_pawn_base[0], 0, 50 );
	MAKE_GENES( candidate_passed_pawn_base, 0, 20, 3, 1 );

	MAKE_GENES( pawn_shield, 0, 100, 3, 0 );
	MAKE_GENES( pawn_shield_attack, 0, 100, 3, 0 );
	MAKE_GENES( absolute_pin, 0, 100, 5, 1 );
	MAKE_GENE( rooks_on_open_file, 0, 100 );
	MAKE_GENE( rooks_on_half_open_file, 0, 100 );
	MAKE_GENE( connected_rooks, 0, 500 );
	MAKE_GENES( tropism, 0, 50, 5, 1 );
	MAKE_GENES( king_attack_by_piece, 0, 100, 5, 1 );
	MAKE_GENES( king_check_by_piece, 0, 100, 4, 2 );
	MAKE_GENE( king_melee_attack_by_rook, 0, 100 );
	MAKE_GENE( king_melee_attack_by_queen, 0, 100 );
	MAKE_GENES( king_attack_min, 0, 1000, 2, 0 );
	MAKE_GENES( king_attack_max, 100, 1500, 2, 0 );
	MAKE_GENES( king_attack_rise, 1, 30, 2, 0 );
	MAKE_GENES( king_attack_exponent, 100, 300, 2, 0 );
	MAKE_GENES( king_attack_offset, 0, 100, 2, 0 );
	MAKE_GENE( king_attack_pawn_shield, 1, 100 );
	MAKE_GENE( center_control, 0, 500 );
	//MAKE_GENES( material_imbalance, -500, 500, 2, 0 );
	MAKE_GENE( rule_of_the_square, 0, 100 );
	MAKE_GENE( passed_pawn_unhindered, 0, 100 );
	MAKE_GENES( defended_by_pawn, 0, 100, 5, 1 );
	MAKE_GENES( attacked_piece, 0, 100, 5, 1 );
	MAKE_GENES( hanging_piece, 0, 100, 5, 1 );
	MAKE_GENE( mobility_knight_min, -100, 0 );
	MAKE_GENE( mobility_knight_max, 0, 100 );
	MAKE_GENE( mobility_knight_rise, 1, 50 );
	MAKE_GENE( mobility_knight_offset, 0, 4 );
	MAKE_GENE( mobility_bishop_min, -100, 0 );
	MAKE_GENE( mobility_bishop_max, 0, 100 );
	MAKE_GENE( mobility_bishop_rise, 1, 50 );
	MAKE_GENE( mobility_bishop_offset, 0, 7 );
	MAKE_GENE( mobility_rook_min, -100, 0 );
	MAKE_GENE( mobility_rook_max, 0, 100 );
	MAKE_GENE( mobility_rook_rise, 1, 50 );
	MAKE_GENE( mobility_rook_offset, 0, 7 );
	MAKE_GENE( mobility_queen_min, -100, 0 );
	MAKE_GENE( mobility_queen_max, 0, 100 );
	MAKE_GENE( mobility_queen_rise, 1, 50 );
	MAKE_GENE( mobility_queen_offset, 0, 14 );
	MAKE_GENE( side_to_move, 0, 100 );
	//MAKE_GENE( drawishness, -500, 0 );
	MAKE_GENE( rooks_on_rank_7, 0, 100 );
	MAKE_GENES( knight_outposts, 0, 100, 2, 0 );
	MAKE_GENES( bishop_outposts, 0, 100, 2, 0 );
	MAKE_GENE( trapped_rook[0].mg(), -100, 0 );
	MAKE_GENE( trapped_rook[1].mg(), -100, 0 );
	MAKE_GENE( trapped_bishop, -100, 0 );
}

struct individual
{
	individual()
		: fitness_()
		, max_diff_()
		, max_diff_pos_()
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

	bool apply()
	{
		for( unsigned int i = 0; i < values_.size(); ++i ) {
			*genes[i].target_ = values_[i];
		}
		eval_values::update_derived();
		if( !eval_values::sane() ) {
			return false;
		}
		if( eval_values::normalize() ) {
			for( unsigned int i = 0; i < genes.size(); ++i ) {
				values_[i] = *genes[i].target_;
			}
		}
		return true;
	}

	void calc_fitness( std::vector<reference_data>& data ) {
		apply();

		fitness_ = 0;
		max_diff_ = 0;
		for( std::size_t i = 0; i < data.size(); ++i ) {
			reference_data& ref = data[i];

			ref.p.update_derived();
			pawn_hash_table.clear( ref.p.pawn_hash );
			short score = evaluate_full( ref.p, ref.c );

#if 0
			double difference = std::abs( ref.avg_eval - static_cast<double>(score) );
#else
			int difference = 0;
			if( ref.min_eval > THRESHOLD ) {
				if( score < THRESHOLD ) {
					difference = THRESHOLD - score;
				}
			}
			else if( ref.max_eval < -THRESHOLD ) {
				if( score > -THRESHOLD ) {
					difference = score + THRESHOLD;
				}
			}
			else {
				if( score < ref.min_eval ) {
					difference = int(ref.min_eval) - score;
				}
				else if( score > ref.max_eval ) {
					difference = int(score) - ref.max_eval;
				}
			}
#endif

			if( score < 0 && ref.min_eval > 0 ) {
				difference *= 2;
			}
			else if( score > 0 && ref.max_eval < 0 ) {
				difference *= 2;
			}

			fitness_ += difference;

			if( static_cast<short>(difference) > max_diff_ ) {
				max_diff_ = static_cast<short>(difference);
				max_diff_pos_ = i;
			}
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
	int max_diff_pos_;
};


typedef std::vector<individual*> population;


bool insert( population& pop, std::set<individual>& seen, individual* i )
{
	bool ret = false;
	if( i->apply() && seen.insert( *i ).second ) {
		pop.push_back( i );
		ret = true;
	}
	else {
		delete i;
	}

	return ret;
}


void mutate( population& pop, std::set<individual>& seen, std::vector<reference_data>& data )
{
	// Randomly mutates elements and adds the resulting mutants to the container
	std::size_t orig_count = pop.size();

	for( int x = 0; x < 2; ++x ) {
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
					insert( pop, seen, mut );
				}
			}
			{
				// Randomly mutate a field, increasing it by one
				unsigned int fi = rand() % genes.size();
				gene_t const& gene = genes[fi];
				if( ref.values_[fi] < gene.max_ ) {
					individual* mut = new individual( ref );
					++mut->values_[fi];
					insert( pop, seen, mut );
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
					insert( pop, seen, mut );
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
	if( orig_count < 2 ) {
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
			int r = rand() % 3;
			if( !r ) {
				child->values_[i] = (i1.values_[i] + i2.values_[i]) / 2;
			}
			else if( r == 1 ) {
				child->values_[i] = i2.values_[i];
			}
		}
		if( insert( pop, seen, child ) ) {
			child->calc_fitness( data );
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
	for( std::size_t i = 0; i < std::min(n, pop.size()); ++i ) {
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
	best.apply();
	for( std::size_t i = 0; i < tweaks.size(); ++i ) {
		tweak_base const& t = *tweaks[i];
		std::cerr << "\t" << t.to_string();
	}
	std::string fen = position_to_fen_noclock( data[best.max_diff_pos_].p, data[best.max_diff_pos_].c );
	std::cout << "New best: " << best.fitness_ << " " << best.max_diff_ << "  " << fen << std::endl;

	best.calc_fitness( data );
}


std::vector<reference_data> load_data()
{
	std::vector<reference_data> ret;

	std::ifstream in_fen("test/testpositions.txt");
	std::ifstream in_scores("test/data.txt");

	int endgames = 0;
	int unknown_endgames = 0;
	int marginal = 0;

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
			entry.min_eval = std::min(entry.min_eval, v);
			entry.max_eval = std::max(entry.max_eval, v);
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

		short endgame = 0;
		if( evaluate_endgame( entry.p, endgame ) ) {
			++endgames;
			continue;
		}

		if( popcount( entry.p.bitboards[0].b[bb_type::all_pieces] | entry.p.bitboards[1].b[bb_type::all_pieces] ) <= 7 ) {
			++unknown_endgames;
			continue;
		}

		if( entry.min_eval > THRESHOLD || entry.max_eval < -THRESHOLD) {
			++marginal;
		}

		entry.avg_eval = score;
		entry.fen = fen;

		ret.push_back(entry);
	}

	std::cout << "Loaded " << ret.size() << " test positions of which " << marginal << " are marginal. Removed " << endgames << " known and " << unknown_endgames << " unknown endgames." << std::endl;
	return ret;
}


void add_random( population & pop, std::set<individual>& seen, std::vector<reference_data>& data, unsigned int count = 10 ) {
	for( unsigned int i = 0; i < count; ++i ) {
		individual* p = new individual;
		p->randomize();
		p->calc_fitness( data );
		insert( pop, seen, p );
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

#if 1
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
