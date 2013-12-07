#if DEVELOPMENT
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
#include "util/string.hpp"
#include "tweak.hpp"
#include "util.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <memory>

int const THRESHOLD = 300;

namespace {

randgen rng;

static calc_result tweak_calc( context& ctx, position& p, duration const& move_time_limit, int clock, seen_positions& seen
		  , new_best_move_callback_base& new_best_cb )
{
	if( clock > 10 ) {
		calc_manager cmgr(ctx);
		return cmgr.calc( p, -1, timestamp(), move_time_limit, move_time_limit, clock, seen, new_best_cb );
	}

	check_map check( p );
	auto moves = calculate_moves<movegen_type::all>( p, check );

	calc_result result;
	if( moves.empty() ) {
		if( check.check ) {
			if( p.white() ) {
				std::cerr << "BLACK WINS" << std::endl;
				result.forecast = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				result.forecast = result::win;
			}
		}
		else {
			if( !p.white() ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			result.forecast = result::draw;
		}
	}
	else {
		result.best_move = moves[rng.get_uint64() % moves.size()];
	}

	return result;
}

static void generate_test_positions_impl( context& ctx )
{
	ctx.conf_.max_moves = 20 + rng.get_uint64() % 70;

	ctx.tt_.clear_data();
	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
	position p;

	unsigned int i = 1;
	seen_positions seen( p.hash_ );

	def_new_best_move_callback cb( ctx.conf_ );

	calc_result result;
	while( !(result = tweak_calc( ctx, p, duration(), i, seen, cb ) ).best_move.empty() ) {
		if( !validate_move( p, result.best_move ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		bool reset_seen = false;
		pieces::type piece = p.get_piece(result.best_move.source());
		pieces::type captured_piece = p.get_captured_piece( result.best_move );
		if( piece == pieces::pawn || captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, result.best_move );

		if( !reset_seen ) {
			seen.push_root( p.hash_ );
		}
		else {
			seen.reset_root( p.hash_ );
		}

		if( seen.depth() > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}

		++i;

		if( ctx.conf_.max_moves && i >= ctx.conf_.max_moves ) {
			std::ofstream out("test/testpositions.txt", std::ofstream::app|std::ofstream::out );

			std::string fen = position_to_fen_noclock( ctx.conf_, p );
			out << fen << std::endl;

			break;
		}
	}
}
}

void generate_test_positions( context& ctx )
{
	ctx.tt_.init( ctx.conf_.memory );	
	ctx.conf_.set_max_search_depth( 6 );
	
	while( true ) {
		generate_test_positions_impl( ctx );
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
	short min_eval;
	short max_eval;
	double avg_eval;
	short forecast;
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
		genes_.push_back(static_cast<int>(genes.size()));
		genes.push_back(gene_t( &target.mg(), min, max ) );
		genes_.push_back(static_cast<int>(genes.size()));
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
		genes_.push_back(static_cast<int>(genes.size()));
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
	MAKE_GENE( material_values[pieces::rook], 420, 690 );
	MAKE_GENE( material_values[pieces::queen], 870, 1500 );
	MAKE_GENE( double_bishop, 10, 100 );

	MAKE_GENE( passed_pawn_advance_power, 100, 210 );
	MAKE_GENES( passed_pawn_king_distance, 0, 10, 2, 0 );

	MAKE_GENE( passed_pawn_base[0], 1, 100 );
	MAKE_GENES( passed_pawn_base, 0, 20, 3, 1 );

	MAKE_GENE( doubled_pawn_base[0][0], 1, 50 );
	MAKE_GENES( doubled_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( doubled_pawn_base[1][0], 1, 50 );
	MAKE_GENES( doubled_pawn_base[1], 0, 20, 3, 1 );


	MAKE_GENE( backward_pawn_base[0][0], 1, 50 );
	MAKE_GENES( backward_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( backward_pawn_base[1][0], 1, 50 );
	MAKE_GENES( backward_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( isolated_pawn_base[0][0], 1, 50 );
	MAKE_GENES( isolated_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( isolated_pawn_base[1][0], 1, 50 );
	MAKE_GENES( isolated_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( connected_pawn_base[0][0], 1, 50 );
	MAKE_GENES( connected_pawn_base[0], 0, 20, 3, 1 );
	MAKE_GENE( connected_pawn_base[1][0], 1, 50 );
	MAKE_GENES( connected_pawn_base[1], 0, 20, 3, 1 );

	MAKE_GENE( candidate_passed_pawn_base[0], 1, 50 );
	MAKE_GENES( candidate_passed_pawn_base, 0, 20, 3, 1 );

	MAKE_GENES( pawn_shield, 1, 80, 4, 0 );
	MAKE_GENES( pawn_shield_attack, 1, 80, 4, 0 );
	MAKE_GENES( absolute_pin, 1, 100, 4, 2 );
	MAKE_GENE( rooks_on_open_file, 1, 100 );
	MAKE_GENE( rooks_on_half_open_file, 1, 100 );
	MAKE_GENE( connected_rooks, 1, 500 );
	MAKE_GENES( tropism, 1, 50, 5, 1 );
	MAKE_GENES( king_attack_by_piece, 1, 50, 5, 1 );
	MAKE_GENES( king_check_by_piece, 1, 50, 4, 2 );
	MAKE_GENE( king_melee_attack_by_rook, 1, 50 );
	MAKE_GENE( king_melee_attack_by_queen, 1, 50 );
	MAKE_GENE( king_attack_pawn_shield, 1, 100 );
	MAKE_GENE( center_control, 1, 400 );
	MAKE_GENES( material_imbalance, 0, 200, 2, 0 );
	MAKE_GENE( rule_of_the_square, 0, 100 );
	MAKE_GENE( passed_pawn_unhindered, 0, 100 );
	MAKE_GENES( defended_by_pawn, 0, 100, 5, 1 );
	MAKE_GENES( attacked_piece, 0, 100, 5, 1 );
	MAKE_GENES( hanging_piece, 1, 100, 5, 1 );
	MAKE_GENES( mobility_rise, 1, 15, 4, 2 );
	MAKE_GENES( mobility_min, 0, 27, 4, 2 );
	MAKE_GENES( mobility_duration, 1, 26, 4, 2 );
	MAKE_GENE( side_to_move, 0, 100 );
	MAKE_GENE( rooks_on_rank_7, 1, 100 );
	MAKE_GENES( knight_outposts, 1, 100, 2, 0 );
	MAKE_GENES( bishop_outposts, 1, 100, 2, 0 );
	MAKE_GENE( trapped_rook[0].mg(), -100, 0 );
	MAKE_GENE( trapped_rook[1].mg(), -120, 0 );
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

	void randomize()
	{
		for( unsigned int i = 0; i < values_.size(); ++i ) {
			gene_t const& gene = genes[i];
			values_[i] = gene.min_ + (rng.get_uint64() % (gene.max_ - gene.min_ + 1));
		}
	}

	bool apply()
	{
		for( unsigned int i = 0; i < values_.size(); ++i ) {
			*genes[i].target_ = values_[i];
		}

		bool changed = false;
		if( !eval_values::sane_base( changed ) ) {
			return false;
		}
		eval_values::update_derived();
		if( !eval_values::sane_derived() ) {
			return false;
		}

		changed |= eval_values::normalize();

		if( changed ) {
			for( unsigned int i = 0; i < genes.size(); ++i ) {
				values_[i] = *genes[i].target_;
			}
		}
		return true;
	}

	int calc_fitness( pawn_structure_hash_table& pawn_tt, std::vector<reference_data>& data ) {
		apply();

		fitness_ = 0;
		max_diff_ = 0;
		int max_diff_pos_ = -1;

		for( std::size_t i = 0; i < data.size(); ++i ) {
			reference_data& ref = data[i];

			ref.p.init_material();
			ref.p.init_eval();
			pawn_tt.clear( ref.p.pawn_hash );
			short score = evaluate_full( pawn_tt, ref.p );

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
			/*if( ref.forecast > ref.max_eval && score < ref.min_eval ) {
				difference *= 2;
			}
			else if( ref.forecast < ref.min_eval && score > ref.max_eval ) {
				difference *= 2;
			}
			if( ref.forecast > ref.max_eval && score > ref.max_eval ) {
				difference /= 2;
			}
			else if( ref.forecast < ref.min_eval && score < ref.min_eval ) {
				difference /= 2;
			}*/
#endif

			/*if( score < 0 && ref.min_eval > 0 ) {
				difference *= 2;
			}
			else if( score > 0 && ref.max_eval < 0 ) {
				difference *= 2;
			}*/

			fitness_ += difference;

			if( static_cast<short>(difference) > max_diff_ ) {
				max_diff_ = static_cast<short>(difference);
				max_diff_pos_ = static_cast<int>(i);
			}
		}
		fitness_ /= data.size();

		return max_diff_pos_;
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


void mutate( pawn_structure_hash_table& pawn_tt, population& pop, std::set<individual>& seen, std::vector<reference_data>& data )
{
	// Randomly mutates elements and adds the resulting mutants to the container
	std::size_t orig_count = pop.size();

	for( int x = 0; x < 2; ++x ) {
		std::size_t loop_count = pop.size();
		for( std::size_t i = 0; i < loop_count; ++i ) {
			individual const& ref = *pop[i];
			{
				// Randomly mutate a field, decreasing it by one
				unsigned int fi = rng.get_uint64() % genes.size();
				gene_t const& gene = genes[fi];
				if( ref.values_[fi] > gene.min_ ) {
					individual* mut = new individual( ref );
					--mut->values_[fi];
					insert( pop, seen, mut );
				}
			}
			{
				// Randomly mutate a field, increasing it by one
				unsigned int fi = rng.get_uint64() % genes.size();
				gene_t const& gene = genes[fi];
				if( ref.values_[fi] < gene.max_ ) {
					individual* mut = new individual( ref );
					++mut->values_[fi];
					insert( pop, seen, mut );
				}
			}
			{
				// Randomly mutate a field using a random value
				unsigned int fi = rng.get_uint64() % genes.size();
				gene_t const& gene = genes[fi];

				short v = gene.min_ + (rng.get_uint64() % (gene.max_ - gene.min_ + 1));
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
		pop[i]->calc_fitness( pawn_tt, data );
	}
}

void combine( pawn_structure_hash_table& pawn_tt, population& pop, std::set<individual>& seen, std::vector<reference_data>& data )
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
			r2 = rng.get_uint64() % orig_count;
		}

		individual const& i1 = *pop[r1];
		individual const& i2 = *pop[r2];

		individual* child( new individual(i1) );
		for( std::size_t i = 0; i < genes.size(); ++i ) {
			int r = rng.get_uint64() % 3;
			if( !r ) {
				child->values_[i] = (i1.values_[i] + i2.values_[i]) / 2;
			}
			else if( r == 1 ) {
				child->values_[i] = i2.values_[i];
			}
		}
		if( insert( pop, seen, child ) ) {
			child->calc_fitness( pawn_tt, data );
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
		std::size_t r = rng.get_uint64() % limit;

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


void save_new_best( context& ctx, individual& best, std::vector<reference_data>& data )
{
	std::cout << std::endl;
	best.apply();
	for( std::size_t i = 0; i < tweaks.size(); ++i ) {
		tweak_base const& t = *tweaks[i];
		std::cerr << "\t" << t.to_string();
	}

	int max_diff_pos = best.calc_fitness( ctx.pawn_tt_, data );

	std::string fen;
	if( max_diff_pos >= 0 && static_cast<std::size_t>(max_diff_pos) < data.size() ) {
		fen = position_to_fen_noclock( ctx.conf_, data[max_diff_pos].p );
	}
	else {
		fen = "unknown";
	}
	std::cout << "New best: " << best.fitness_ << " " << best.max_diff_ << "  " << fen << std::endl;
}


std::vector<reference_data> load_data( context& ctx )
{
	std::vector<reference_data> ret;

	std::ifstream in_fen(ctx.conf_.self_dir + "test/testpositions.txt");
	std::ifstream in_scores(ctx.conf_.self_dir + "test/data.txt");
	//std::ifstream forecasts(ctx.conf_.self_dir + "test/forecast.txt");

	int endgames = 0;
	int unknown_endgames = 0;
	int marginal = 0;
	int duplicate = 0;

	std::string mode;
	std::getline( in_scores, mode );

	bool modes[100] = {0};

	{
		std::istringstream ss(mode);
		std::string m;
		int i = 0;
		while( (ss >> m) ) {
			if( m == "abs" ) {
				modes[i] = true;
			}
			else if( m == "rel" ) {
				modes[i] = false;
			}
			else {
				abort();
			}
			++i;
		}
	}

	std::set<uint64_t> seen;

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		std::string score_line;
		std::getline( in_scores, score_line );
		//std::string forecast_line;
		//std::getline( forecasts, forecast_line );

		short score = 0;
		int count = 0;

		reference_data entry;
		entry.min_eval = result::win;
		entry.max_eval = result::loss;
		//to_int<short>( forecast_line, entry.forecast );

		if( score_line.empty() ) {
			++marginal;
			continue;
		}

		if( !parse_fen( ctx.conf_, fen, entry.p ) ) {
			abort();
		}

		if( !seen.insert( entry.p.hash_ ).second ) {
			++duplicate;
			continue;
		}
		
		short endgame = 0;
		if( evaluate_endgame( entry.p, endgame ) ) {
			++endgames;
			continue;
		}

		std::istringstream ss(score_line);
		ss.imbue( std::locale::classic() );
		double vd;
		while( (ss >> vd) ) {
			vd *= 100;
			if( modes[count] && entry.p.black() ) {
				vd = -vd;
			}
			short v = static_cast<short>(vd);
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
		if( popcount( entry.p.bitboards[0][bb_type::all_pieces] | entry.p.bitboards[1][bb_type::all_pieces] ) <= 7 ) {
			++unknown_endgames;
			continue;
		}

		if( entry.min_eval > THRESHOLD || entry.max_eval < -THRESHOLD) {
			++marginal;
			continue;
		}

		entry.avg_eval = score;
		entry.fen = fen;

		ret.push_back(entry);
	}

	std::cout << "Loaded " << ret.size() << " test positions of which " << marginal << " are marginal. Removed " << duplicate << " duplicates. Removed " << endgames << " known and " << unknown_endgames << " unknown endgames." << std::endl;
	return ret;
}


void add_random( pawn_structure_hash_table& pawn_tt, population & pop, std::set<individual>& seen, std::vector<reference_data>& data, unsigned int count = 10 ) {
	for( unsigned int i = 0; i < count; ++i ) {
		individual* p = new individual;
		p->randomize();
		p->calc_fitness( pawn_tt, data );
		insert( pop, seen, p );
	}
}


void tweak_evaluation( context& ctx )
{
	rng.seed();
	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size(), true );
	std::vector<reference_data> data = load_data( ctx );

	init_genes();

	double original_fitness = 999999;
	double best_fitness = original_fitness;

	population pop;
	std::set<individual> seen;

#if 1
	pop.push_back( new individual() );
	pop[0]->calc_fitness( ctx.pawn_tt_, data );
	seen.insert( *pop[0] );
#else
	add_random( ctx.pawn_tt_, pop, seen, data, 50 );
#endif

	while( true ) {
		std::cout << ".";
		std::flush( std::cout );
		srand ( static_cast<unsigned int>(time(NULL)) );

		mutate( ctx.pawn_tt_, pop, seen, data );
		add_random( ctx.pawn_tt_, pop, seen, data );
		combine( ctx.pawn_tt_, pop, seen, data );
		sort( pop );
		select( pop, seen );

		if( pop.front()->fitness_ < best_fitness ) {
			save_new_best( ctx, *pop.front(), data );
			best_fitness = pop.front()->fitness_;
			for( std::size_t i = 1; i < pop.size(); ++i ) {
				delete pop[i];
			}
			pop.resize(1);
			seen.clear();
			seen.insert( *pop[0] );
			rng.seed();
		}
	}
}
#endif
