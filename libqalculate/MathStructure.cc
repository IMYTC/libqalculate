/*
    Qalculate (library)

    Copyright (C) 2003-2007, 2008, 2016-2017  Hanna Knutsson (hanna.knutsson@protonmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "MathStructure.h"
#include "Calculator.h"
#include "Number.h"
#include "Function.h"
#include "Variable.h"
#include "Unit.h"
#include "Prefix.h"
#include <map>
#include <algorithm>

#define SWAP_CHILDREN(i1, i2)		{MathStructure *swap_mstruct = v_subs[v_order[i1]]; v_subs[v_order[i1]] = v_subs[v_order[i2]]; v_subs[v_order[i2]] = swap_mstruct;}
#define CHILD_TO_FRONT(i)		v_order.insert(v_order.begin(), v_order[i]); v_order.erase(v_order.begin() + (i + 1));
#define SET_CHILD_MAP(i)		setToChild(i + 1, true);
#define SET_MAP(o)			set(o, true);
#define SET_MAP_NOCOPY(o)		set_nocopy(o, true);
#define MERGE_APPROX_AND_PREC(o)	if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define CHILD_UPDATED(i)		if(!b_approx && CHILD(i).isApproximate()) b_approx = true; if(CHILD(i).precision() > 0 && (i_precision < 1 || CHILD(i).precision() < i_precision)) i_precision = CHILD(i).precision();
#define CHILDREN_UPDATED		for(size_t child_i = 0; child_i < SIZE; child_i++) {if(!b_approx && CHILD(child_i).isApproximate()) b_approx = true; if(CHILD(child_i).precision() > 0 && (i_precision < 1 || CHILD(child_i).precision() < i_precision)) i_precision = CHILD(child_i).precision();}

#define APPEND(o)		v_order.push_back(v_subs.size()); v_subs.push_back(new MathStructure(o)); if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define APPEND_NEW(o)		{v_order.push_back(v_subs.size()); MathStructure *m_append_new = new MathStructure(o); v_subs.push_back(m_append_new); if(!b_approx && m_append_new->isApproximate())	b_approx = true; if(m_append_new->precision() > 0 && (i_precision < 1 || m_append_new->precision() < i_precision)) i_precision = m_append_new->precision();}
#define APPEND_COPY(o)		v_order.push_back(v_subs.size()); v_subs.push_back(new MathStructure(*(o))); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define APPEND_POINTER(o)	v_order.push_back(v_subs.size()); v_subs.push_back(o); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define APPEND_REF(o)		v_order.push_back(v_subs.size()); v_subs.push_back(o); (o)->ref(); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define PREPEND(o)		v_order.insert(v_order.begin(), v_subs.size()); v_subs.push_back(new MathStructure(o)); if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define PREPEND_REF(o)		v_order.insert(v_order.begin(), v_subs.size()); v_subs.push_back(o); (o)->ref(); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define INSERT_REF(o, i)	v_order.insert(v_order.begin() + i, v_subs.size()); v_subs.push_back(o); (o)->ref(); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define CLEAR			v_order.clear(); for(size_t i = 0; i < v_subs.size(); i++) {v_subs[i]->unref();} v_subs.clear();
//#define REDUCE(v_size)		for(size_t v_index = v_size; v_index < v_order.size(); v_index++) {v_subs[v_order[v_index]]->unref(); v_subs.erase(v_subs.begin() + v_order[v_index]);} v_order.resize(v_size);
#define REDUCE(v_size)          {vector<size_t> v_tmp; v_tmp.resize(SIZE, 0); for(size_t v_index = v_size; v_index < v_order.size(); v_index++) {v_subs[v_order[v_index]]->unref(); v_subs[v_order[v_index]] = NULL; v_tmp[v_order[v_index]] = 1;} v_order.resize(v_size); for(vector<MathStructure*>::iterator v_it = v_subs.begin(); v_it != v_subs.end();) {if(*v_it == NULL) v_it = v_subs.erase(v_it); else ++v_it;} size_t i_change = 0; for(size_t v_index = 0; v_index < v_tmp.size(); v_index++) {if(v_tmp[v_index] == 1) i_change++; v_tmp[v_index] = i_change;} for(size_t v_index = 0; v_index < v_order.size(); v_index++) v_order[v_index] -= v_tmp[v_index];}
#define CHILD(v_index)		(*v_subs[v_order[v_index]])
#define SIZE			v_order.size()
#define LAST			(*v_subs[v_order[v_order.size() - 1]])
#define ERASE(v_index)		v_subs[v_order[v_index]]->unref(); v_subs.erase(v_subs.begin() + v_order[v_index]); for(size_t v_index2 = 0; v_index2 < v_order.size(); v_index2++) {if(v_order[v_index2] > v_order[v_index]) v_order[v_index2]--;} v_order.erase(v_order.begin() + (v_index));

#define IS_REAL(o)		(o.isNumber() && o.number().isReal())
#define IS_RATIONAL(o)		(o.isNumber() && o.number().isRational())

#define IS_A_SYMBOL(o)		((o.isSymbolic() || o.isVariable() || o.isFunction()) && o.representsScalar())

#define POWER_CLEAN(o)		if(o[1].isOne()) {o.setToChild(1);} else if(o[1].isZero()) {o.set(1, 1, 0);}

#define VALID_ROOT(o)		(o.size() == 2 && o[1].isNumber() && o[1].number().isInteger() && o[1].number().isPositive())
#define THIS_VALID_ROOT		(SIZE == 2 && CHILD(1).isNumber() && CHILD(1).number().isInteger() && CHILD(1).number().isPositive())

/*void printRecursive(const MathStructure &mstruct) {
	std::cout << "RECURSIVE " << mstruct.print() << std::endl;
	for(size_t i = 0; i < mstruct.size(); i++) {
		std::cout << i << ": " << mstruct[i].print() << std::endl;
		for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
			std::cout << i << "-" << i2 << ": " << mstruct[i][i2].print() << std::endl;
			for(size_t i3 = 0; i3 < mstruct[i][i2].size(); i3++) {
				std::cout << i << "-" << i2 << "-" << i3 << ": " << mstruct[i][i2][i3].print() << std::endl;
				for(size_t i4 = 0; i4 < mstruct[i][i2][i3].size(); i4++) {
					std::cout << i << "-" << i2 << "-" << i3 << "-" << i4 << ": " << mstruct[i][i2][i3][i4].print() << std::endl;
					for(size_t i5 = 0; i5 < mstruct[i][i2][i3][i4].size(); i5++) {
						std::cout << i << "-" << i2 << "-" << i3 << "-" << i4 << "-" << i5 << ": " << mstruct[i][i2][i3][i4][i5].print() << std::endl;
						for(size_t i6 = 0; i6 < mstruct[i][i2][i3][i4][i5].size(); i6++) {
							std::cout << i << "-" << i2 << "-" << i3 << "-" << i4 << "-" << i5 << "-" << i6 << ": " << mstruct[i][i2][i3][i4][i5][i6].print() << std::endl;
						}
					}
				}
			}
		}
	}
}*/

struct sym_desc {
	MathStructure sym;
	Number deg_a;
	Number deg_b;
	Number ldeg_a;
	Number ldeg_b;
	Number max_deg;
	size_t max_lcnops;
	bool operator<(const sym_desc &x) const {
		if (max_deg == x.max_deg) return max_lcnops < x.max_lcnops;
		else return max_deg.isLessThan(x.max_deg);
	}
};
typedef std::vector<sym_desc> sym_desc_vec;

bool polynomial_long_division(const MathStructure &mnum, const MathStructure &mden, const MathStructure &xvar_pre, MathStructure &mquotient, MathStructure &mrem, const EvaluationOptions &eo, bool check_args = false, bool for_newtonraphson = false);
void integer_content(const MathStructure &mpoly, Number &icontent);
void interpolate(const MathStructure &gamma, const Number &xi, const MathStructure &xvar, MathStructure &minterp, const EvaluationOptions &eo);
bool get_first_symbol(const MathStructure &mpoly, MathStructure &xvar);
bool divide_in_z(const MathStructure &mnum, const MathStructure &mden, MathStructure &mquotient, const sym_desc_vec &sym_stats, size_t var_i, const EvaluationOptions &eo);
bool prem(const MathStructure &mnum, const MathStructure &mden, const MathStructure &xvar, MathStructure &mrem, const EvaluationOptions &eo, bool check_args = true);
bool sr_gcd(const MathStructure &m1, const MathStructure &m2, MathStructure &mgcd, const sym_desc_vec &sym_stats, size_t var_i, const EvaluationOptions &eo);
void polynomial_smod(const MathStructure &mpoly, const Number &xi, MathStructure &msmod, const EvaluationOptions &eo, MathStructure *mparent = NULL, size_t index_smod = 0);
bool heur_gcd(const MathStructure &m1, const MathStructure &m2, MathStructure &mgcd, const EvaluationOptions &eo, MathStructure *ca, MathStructure *cb, const sym_desc_vec &sym_stats, size_t var_i);
void add_symbol(const MathStructure &mpoly, sym_desc_vec &v);
void collect_symbols(const MathStructure &mpoly, sym_desc_vec &v);
void add_symbol(const MathStructure &mpoly, vector<MathStructure> &v);
void collect_symbols(const MathStructure &mpoly, vector<MathStructure> &v);
void get_symbol_stats(const MathStructure &m1, const MathStructure &m2, sym_desc_vec &v);
bool sqrfree(MathStructure &mpoly, const EvaluationOptions &eo);
bool sqrfree(MathStructure &mpoly, const vector<MathStructure> &symbols, const EvaluationOptions &eo);

bool flattenMultiplication(MathStructure &mstruct) {
	bool retval = false;
	for(size_t i = 0; i < mstruct.size();) {
		if(mstruct[i].isMultiplication()) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				mstruct[i][i2].ref();
				mstruct.insertChild_nocopy(&mstruct[i][i2], i + i2 + 2);
			}
			mstruct.delChild(i + 1);
			retval = true;
		} else {
			i++;
		}
	}
	return retval;
}

bool warn_about_assumed_not_value(const MathStructure &mstruct, const MathStructure &mvalue, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = false;
	eo2.test_comparisons = true;
	eo2.isolate_x = true;
	eo2.expand = true;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	MathStructure mnonzero(mstruct);
	mnonzero.add(mvalue, OPERATION_NOT_EQUALS);
	mnonzero.eval(eo2);
	if(mnonzero.isZero()) return false;
	if(mnonzero.isOne()) return true;
	if(mvalue.isZero() && mnonzero.isComparison() && mnonzero.comparisonType() == COMPARISON_NOT_EQUALS && mnonzero[1].isZero() && mnonzero[0].representsApproximatelyZero(true)) return false;
	PrintOptions po;
	po.spell_out_logical_operators = true;
	mnonzero.format(po);
	CALCULATOR->error(false, _("Required assumption: %s."), mnonzero.print(po).c_str(), NULL);
	return true;
}
bool warn_about_denominators_assumed_nonzero(const MathStructure &mstruct, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = false;
	eo2.test_comparisons = true;
	eo2.isolate_x = true;
	eo2.expand = true;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	MathStructure mnonzero(mstruct);
	mnonzero.add(m_zero, OPERATION_NOT_EQUALS);
	mnonzero.eval(eo2);
	if(mnonzero.isZero()) return false;
	if(mnonzero.isOne()) return true;
	if(mnonzero.isComparison() && mnonzero.comparisonType() == COMPARISON_NOT_EQUALS && mnonzero[1].isZero() && mnonzero[0].representsApproximatelyZero(true)) return false;
	PrintOptions po;
	po.spell_out_logical_operators = true;
	mnonzero.format(po);
	CALCULATOR->error(false, _("To avoid division by zero, the following must be true: %s."), mnonzero.print(po).c_str(), NULL);
	return true;
}
bool warn_about_denominators_assumed_nonzero_or_positive(const MathStructure &mstruct, const MathStructure &mstruct2, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = false;
	eo2.test_comparisons = true;
	eo2.isolate_x = true;
	eo2.expand = true;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	MathStructure mnonzero(mstruct);
	mnonzero.add(m_zero, OPERATION_NOT_EQUALS);
	MathStructure *mpos = new MathStructure(mstruct2);
	mpos->add(m_zero, OPERATION_EQUALS_GREATER);
	mnonzero.add_nocopy(mpos, OPERATION_LOGICAL_OR);
	mnonzero.eval(eo2);
	if(mnonzero.isZero()) return false;
	if(mnonzero.isOne()) return true;
	if(mnonzero.isComparison() && mnonzero.comparisonType() == COMPARISON_NOT_EQUALS && mnonzero[1].isZero() && mnonzero[0].representsApproximatelyZero(true)) return false;
	PrintOptions po;
	po.spell_out_logical_operators = true;
	mnonzero.format(po);
	CALCULATOR->error(false, _("To avoid division by zero, the following must be true: %s."), mnonzero.print(po).c_str(), NULL);
	return true;
}
bool warn_about_denominators_assumed_nonzero_llgg(const MathStructure &mstruct, const MathStructure &mstruct2, const MathStructure &mstruct3, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = false;
	eo2.test_comparisons = true;
	eo2.isolate_x = true;
	eo2.expand = true;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	MathStructure mnonzero(mstruct);
	mnonzero.add(m_zero, OPERATION_NOT_EQUALS);
	MathStructure *mpos = new MathStructure(mstruct2);
	mpos->add(m_zero, OPERATION_EQUALS_GREATER);
	MathStructure *mpos2 = new MathStructure(mstruct3);
	mpos2->add(m_zero, OPERATION_EQUALS_GREATER);
	mpos->add_nocopy(mpos2, OPERATION_LOGICAL_AND);
	mnonzero.add_nocopy(mpos, OPERATION_LOGICAL_OR);
	MathStructure *mneg = new MathStructure(mstruct2);
	mneg->add(m_zero, OPERATION_LESS);
	MathStructure *mneg2 = new MathStructure(mstruct3);
	mneg2->add(m_zero, OPERATION_LESS);
	mneg->add_nocopy(mneg2, OPERATION_LOGICAL_AND);
	mnonzero.add_nocopy(mneg, OPERATION_LOGICAL_OR);
	mnonzero.eval(eo2);
	if(mnonzero.isZero()) return false;
	if(mnonzero.isOne()) return true;
	if(mnonzero.isComparison() && mnonzero.comparisonType() == COMPARISON_NOT_EQUALS && mnonzero[1].isZero() && mnonzero[0].representsApproximatelyZero(true)) return false;
	PrintOptions po;
	po.spell_out_logical_operators = true;
	mnonzero.format(po);
	CALCULATOR->error(false, _("To avoid division by zero, the following must be true: %s."), mnonzero.print(po).c_str(), NULL);
	return true;
}

void unnegate_sign(MathStructure &mstruct) {
	if(mstruct.isNumber()) {
		mstruct.number().negate();
	} else if(mstruct.isMultiplication()) {
		if(mstruct[0].isMinusOne()) {
			if(mstruct.size() > 2) {
				mstruct.delChild(1);
			} else if(mstruct.size() == 2) {
				mstruct.setToChild(2);
			} else {
				mstruct.set(1, 1, 0);
			}
		} else {
			unnegate_sign(mstruct[0]);
		}
	}
}

void MathStructure::mergePrecision(const MathStructure &o) {MERGE_APPROX_AND_PREC(o)}
void MathStructure::mergePrecision(bool approx, int prec) {
	if(!b_approx && approx) setApproximate(); 
	if(prec >= 0 && (i_precision < 0 || prec < i_precision)) {
		setPrecision(prec);
	}
}

void idm1(const MathStructure &mnum, bool &bfrac, bool &bint);
void idm2(const MathStructure &mnum, bool &bfrac, bool &bint, Number &nr);
void idm3(MathStructure &mnum, Number &nr, bool expand);

void MathStructure::ref() {
	i_ref++;
}
void MathStructure::unref() {
	i_ref--;
	if(i_ref == 0) {
		delete this;
	}
}
size_t MathStructure::refcount() const {
	return i_ref;
}


inline void MathStructure::init() {
	m_type = STRUCT_NUMBER;
	b_approx = false;
	i_precision = -1;
	i_ref = 1;
	function_value = NULL;
	b_protected = false;
	o_variable = NULL;
	o_function = NULL;
	o_unit = NULL;
	o_prefix = NULL;
}

MathStructure::MathStructure() {
	init();
}
MathStructure::MathStructure(const MathStructure &o) {
	init();
	switch(o.type()) {
		case STRUCT_NUMBER: {
			o_number.set(o.number());
			break;
		}
		case STRUCT_ABORTED: {}
		case STRUCT_SYMBOLIC: {
			s_sym = o.symbol();
			break;
		}
		case STRUCT_FUNCTION: {			
			o_function = o.function();
			if(o_function) o.function()->ref();
			if(o.functionValue()) function_value = new MathStructure(*o.functionValue());
			break;
		}
		case STRUCT_VARIABLE: {			
			o_variable = o.variable();
			if(o_variable) o_variable->ref();
			break;
		}
		case STRUCT_UNIT: {
			o_unit = o.unit();
			o_prefix = o.prefix();
			if(o_unit) o_unit->ref();
			b_plural = o.isPlural();
			break;
		}
		case STRUCT_COMPARISON: {
			ct_comp = o.comparisonType();
			break;
		}
		default: {}
	}
	b_protected = o.isProtected();
	for(size_t i = 0; i < o.size(); i++) {
		APPEND_COPY((&o[i]))
	}
	b_approx = o.isApproximate();
	i_precision = o.precision();
	m_type = o.type();
}
MathStructure::MathStructure(long int num, long int den, long int exp10) {
	init();
	o_number.set(num, den, exp10);
}
MathStructure::MathStructure(int num, int den, int exp10) {
	init();
	o_number.set(num, den, exp10);
}
MathStructure::MathStructure(string sym) {
	init();
	if (sym == "undefined") {
		setUndefined(true);
		return;
	}
	s_sym = sym;
	m_type = STRUCT_SYMBOLIC;
}
MathStructure::MathStructure(double float_value) {
	init();
	o_number.setFloat(float_value);
	b_approx = o_number.isApproximate();
	i_precision = o_number.precision();
}
MathStructure::MathStructure(const MathStructure *o, ...) {
	init();
	va_list ap;
	va_start(ap, o); 
	if(o) {
		APPEND_COPY(o)
		while(true) {
			o = va_arg(ap, const MathStructure*);
			if(!o) break;
			APPEND_COPY(o)
		}
	}
	va_end(ap);	
	m_type = STRUCT_VECTOR;
}
MathStructure::MathStructure(MathFunction *o, ...) {
	init();
	va_list ap;
	va_start(ap, o); 
	o_function = o;
	if(o_function) o_function->ref();
	while(true) {
		const MathStructure *mstruct = va_arg(ap, const MathStructure*);
		if(!mstruct) break;
		APPEND_COPY(mstruct)
	}
	va_end(ap);	
	m_type = STRUCT_FUNCTION;
}
MathStructure::MathStructure(Unit *u, Prefix *p) {
	init();
	o_unit = u;
	o_prefix = p;
	if(o_unit) o_unit->ref();
	m_type = STRUCT_UNIT;
}
MathStructure::MathStructure(Variable *o) {
	init();
	o_variable = o;
	if(o_variable) o_variable->ref();
	m_type = STRUCT_VARIABLE;
}
MathStructure::MathStructure(const Number &o) {
	init();
	o_number.set(o);
	b_approx = o_number.isApproximate();
	i_precision = o_number.precision();
}
MathStructure::~MathStructure() {
	clear();
}

void MathStructure::set(const MathStructure &o, bool merge_precision) {
	clear(merge_precision);
	switch(o.type()) {
		case STRUCT_NUMBER: {
			o_number.set(o.number());
			break;
		}
		case STRUCT_ABORTED: {}
		case STRUCT_SYMBOLIC: {
			s_sym = o.symbol();
			break;
		}
		case STRUCT_FUNCTION: {			
			o_function = o.function();
			if(o_function) o.function()->ref();
			if(o.functionValue()) function_value = new MathStructure(*o.functionValue());
			break;
		}
		case STRUCT_VARIABLE: {			
			o_variable = o.variable();
			if(o_variable) o_variable->ref();
			break;
		}
		case STRUCT_UNIT: {
			o_unit = o.unit();
			o_prefix = o.prefix();
			if(o_unit) o_unit->ref();
			b_plural = o.isPlural();
			break;
		}
		case STRUCT_COMPARISON: {
			ct_comp = o.comparisonType();
			break;
		}
		default: {}
	}
	b_protected = o.isProtected();
	for(size_t i = 0; i < o.size(); i++) {
		APPEND_COPY((&o[i]))
	}
	if(merge_precision) {
		MERGE_APPROX_AND_PREC(o);
	} else {
		b_approx = o.isApproximate();
		i_precision = o.precision();
	}
	m_type = o.type();
}
void MathStructure::set_nocopy(MathStructure &o, bool merge_precision) {
	o.ref();
	clear(merge_precision);
	switch(o.type()) {
		case STRUCT_NUMBER: {
			o_number.set(o.number());
			break;
		}
		case STRUCT_ABORTED: {}
		case STRUCT_SYMBOLIC: {
			s_sym = o.symbol();
			break;
		}
		case STRUCT_FUNCTION: {
			o_function = o.function();
			if(o_function) o_function->ref();
			if(o.functionValue()) {
				function_value = (MathStructure*) o.functionValue();
				function_value->ref();
			}
			break;
		}
		case STRUCT_VARIABLE: {
			o_variable = o.variable();
			if(o_variable) o_variable->ref();
			break;
		}
		case STRUCT_UNIT: {
			o_unit = o.unit();
			o_prefix = o.prefix();
			if(o_unit) o_unit->ref();
			b_plural = o.isPlural();
			break;
		}
		case STRUCT_COMPARISON: {
			ct_comp = o.comparisonType();
			break;
		}
		default: {}
	}
	b_protected = o.isProtected();
	for(size_t i = 0; i < o.size(); i++) {
		APPEND_REF((&o[i]))
	}
	if(merge_precision) {
		MERGE_APPROX_AND_PREC(o);
	} else {
		b_approx = o.isApproximate();
		i_precision = o.precision();
	}
	m_type = o.type();
	o.unref();
}
void MathStructure::setToChild(size_t index, bool preserve_precision, MathStructure *mparent, size_t index_this) {
	if(index > 0 && index <= SIZE) {
		if(mparent) {
			CHILD(index - 1).ref();
			mparent->setChild_nocopy(&CHILD(index - 1), index_this, preserve_precision);
		} else {
			set_nocopy(CHILD(index - 1), preserve_precision);
		}
	}
}
void MathStructure::set(long int num, long int den, long int exp10, bool preserve_precision) {
	clear(preserve_precision);
	o_number.set(num, den, exp10);
	if(!preserve_precision) {
		b_approx = false;
		i_precision = -1;
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::set(int num, int den, int exp10, bool preserve_precision) {
	clear(preserve_precision);
	o_number.set(num, den, exp10);
	if(!preserve_precision) {
		b_approx = false;
		i_precision = -1;
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::set(double float_value, bool preserve_precision) {
	clear(preserve_precision);
	o_number.setFloat(float_value);
	if(preserve_precision) {
		MERGE_APPROX_AND_PREC(o_number);
	} else {
		b_approx = o_number.isApproximate();
		i_precision = o_number.precision();
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::set(string sym, bool preserve_precision) {
	if(sym == "undefined") {
		setUndefined(true);
		return;
	}
	clear(preserve_precision);
	s_sym = sym;
	m_type = STRUCT_SYMBOLIC;
}
void MathStructure::setVector(const MathStructure *o, ...) {
	clear();
	va_list ap;
	va_start(ap, o); 
	if(o) {
		APPEND_COPY(o)
		while(true) {
			o = va_arg(ap, const MathStructure*);
			if(!o) break;
			APPEND_COPY(o)
		}
	}
	va_end(ap);	
	m_type = STRUCT_VECTOR;
}
void MathStructure::set(MathFunction *o, ...) {
	clear();
	va_list ap;
	va_start(ap, o); 
	o_function = o;
	if(o_function) o_function->ref();
	while(true) {
		const MathStructure *mstruct = va_arg(ap, const MathStructure*);
		if(!mstruct) break;
		APPEND_COPY(mstruct)
	}
	va_end(ap);	
	m_type = STRUCT_FUNCTION;
}
void MathStructure::set(Unit *u, Prefix *p, bool preserve_precision) {
	clear(preserve_precision);
	o_unit = u;
	o_prefix = p;
	if(o_unit) o_unit->ref();
	m_type = STRUCT_UNIT;
}
void MathStructure::set(Variable *o, bool preserve_precision) {
	clear(preserve_precision);
	o_variable = o;
	if(o_variable) o_variable->ref();
	m_type = STRUCT_VARIABLE;
}
void MathStructure::set(const Number &o, bool preserve_precision) {
	clear(preserve_precision);
	o_number.set(o);
	if(preserve_precision) {
		MERGE_APPROX_AND_PREC(o_number);
	} else {
		b_approx = o_number.isApproximate();
		i_precision = o_number.precision();
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::setUndefined(bool preserve_precision) {
	clear(preserve_precision);
	m_type = STRUCT_UNDEFINED;
}
void MathStructure::setAborted(bool preserve_precision) {
	clear(preserve_precision);
	m_type = STRUCT_ABORTED;
	s_sym = _("aborted");
}

void MathStructure::setProtected(bool do_protect) {b_protected = do_protect;}
bool MathStructure::isProtected() const {return b_protected;}

void MathStructure::operator = (const MathStructure &o) {set(o);}
void MathStructure::operator = (const Number &o) {set(o);}
void MathStructure::operator = (int i) {set(i, 1, 0);}
void MathStructure::operator = (Unit *u) {set(u);}
void MathStructure::operator = (Variable *v) {set(v);}
void MathStructure::operator = (string sym) {set(sym);}
MathStructure MathStructure::operator - () const {
	MathStructure o2(*this);
	o2.negate();
	return o2;
}
MathStructure MathStructure::operator * (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.multiply(o);
	return o2;
}
MathStructure MathStructure::operator / (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.divide(o);
	return o2;
}
MathStructure MathStructure::operator + (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o);
	return o;
}
MathStructure MathStructure::operator - (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.subtract(o);
	return o2;
}
MathStructure MathStructure::operator ^ (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.raise(o);
	return o2;
}
MathStructure MathStructure::operator && (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o, OPERATION_LOGICAL_AND);
	return o2;
}
MathStructure MathStructure::operator || (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o, OPERATION_LOGICAL_OR);
	return o2;
}
MathStructure MathStructure::operator ! () const {
	MathStructure o2(*this);
	o2.setLogicalNot();
	return o2;
}

void MathStructure::operator *= (const MathStructure &o) {multiply(o);}
void MathStructure::operator /= (const MathStructure &o) {divide(o);}
void MathStructure::operator += (const MathStructure &o) {add(o);}
void MathStructure::operator -= (const MathStructure &o) {subtract(o);}
void MathStructure::operator ^= (const MathStructure &o) {raise(o);}

void MathStructure::operator *= (const Number &o) {multiply(o);}
void MathStructure::operator /= (const Number &o) {divide(o);}
void MathStructure::operator += (const Number &o) {add(o);}
void MathStructure::operator -= (const Number &o) {subtract(o);}
void MathStructure::operator ^= (const Number &o) {raise(o);}
		
void MathStructure::operator *= (int i) {multiply(i);}
void MathStructure::operator /= (int i) {divide(i);}
void MathStructure::operator += (int i) {add(i);}
void MathStructure::operator -= (int i) {subtract(i);}
void MathStructure::operator ^= (int i) {raise(i);}
		
void MathStructure::operator *= (Unit *u) {multiply(u);}
void MathStructure::operator /= (Unit *u) {divide(u);}
void MathStructure::operator += (Unit *u) {add(u);}
void MathStructure::operator -= (Unit *u) {subtract(u);}
void MathStructure::operator ^= (Unit *u) {raise(u);}
		
void MathStructure::operator *= (Variable *v) {multiply(v);}
void MathStructure::operator /= (Variable *v) {divide(v);}
void MathStructure::operator += (Variable *v) {add(v);}
void MathStructure::operator -= (Variable *v) {subtract(v);}
void MathStructure::operator ^= (Variable *v) {raise(v);}
		
void MathStructure::operator *= (string sym) {multiply(sym);}
void MathStructure::operator /= (string sym) {divide(sym);}
void MathStructure::operator += (string sym) {add(sym);}
void MathStructure::operator -= (string sym) {subtract(sym);}
void MathStructure::operator ^= (string sym) {raise(sym);}

bool MathStructure::operator == (const MathStructure &o) const {return equals(o);}
bool MathStructure::operator == (const Number &o) const {return equals(o);}
bool MathStructure::operator == (int i) const {return equals(i);}
bool MathStructure::operator == (Unit *u) const {return equals(u);}
bool MathStructure::operator == (Variable *v) const {return equals(v);}
bool MathStructure::operator == (string sym) const {return equals(sym);}

bool MathStructure::operator != (const MathStructure &o) const {return !equals(o);}

/*MathStructure& MathStructure::CHILD(size_t v_index) const {
	if(v_index < v_order.size() && v_order[v_index] < v_subs.size()) return *v_subs[v_order[v_index]];
	MathStructure* m = new MathStructure;//(new UnknownVariable("x","x"));
	m->setUndefined(true);
	return *m;
}*/

const MathStructure &MathStructure::operator [] (size_t index) const {return CHILD(index);}
MathStructure &MathStructure::operator [] (size_t index) {return CHILD(index);}

MathStructure &MathStructure::last() {return LAST;}
const MathStructure MathStructure::last() const {return LAST;}

void MathStructure::clear(bool preserve_precision) {
	m_type = STRUCT_NUMBER;
	o_number.clear();
	if(function_value) {
		function_value->unref();
		function_value = NULL;
	}
	if(o_function) o_function->unref();
	o_function = NULL;
	if(o_variable) o_variable->unref();
	o_variable = NULL;
	if(o_unit) o_unit->unref();
	o_unit = NULL;
	o_prefix = NULL;
	b_plural = false;
	b_protected = false;
	CLEAR;
	if(!preserve_precision) {
		i_precision = -1;
		b_approx = false;
	}
}
void MathStructure::clearVector(bool preserve_precision) {
	clear(preserve_precision);
	m_type = STRUCT_VECTOR;
}
void MathStructure::clearMatrix(bool preserve_precision) {
	clearVector(preserve_precision);
	MathStructure *mstruct = new MathStructure();
	mstruct->clearVector();
	APPEND_POINTER(mstruct);
}

const MathStructure *MathStructure::functionValue() const {
	return function_value;
}

const Number &MathStructure::number() const {
	return o_number;
}
Number &MathStructure::number() {
	return o_number;
}
void MathStructure::numberUpdated() {
	if(m_type != STRUCT_NUMBER) return;
	MERGE_APPROX_AND_PREC(o_number)
}
void MathStructure::childUpdated(size_t index, bool recursive) {
	if(index > SIZE || index < 1) return;
	if(recursive) CHILD(index - 1).childrenUpdated(true);
	MERGE_APPROX_AND_PREC(CHILD(index - 1))
}
void MathStructure::childrenUpdated(bool recursive) {
	for(size_t i = 0; i < SIZE; i++) {
		if(recursive) CHILD(i).childrenUpdated(true);
		MERGE_APPROX_AND_PREC(CHILD(i))
	}
}
const string &MathStructure::symbol() const {
	return s_sym;
}
ComparisonType MathStructure::comparisonType() const {
	return ct_comp;
}
void MathStructure::setComparisonType(ComparisonType comparison_type) {
	ct_comp = comparison_type;
}
void MathStructure::setType(StructureType mtype) {
	m_type = mtype;
}
Unit *MathStructure::unit() const {
	return o_unit;
}
Unit *MathStructure::unit_exp_unit() const {
	if(isUnit()) return o_unit;
	if(isPower() && CHILD(0).isUnit()) return CHILD(0).unit();
	return NULL;
}
Prefix *MathStructure::prefix() const {
	return o_prefix;
}
Prefix *MathStructure::unit_exp_prefix() const {
	if(isUnit()) return o_prefix;
	if(isPower() && CHILD(0).isUnit()) return CHILD(0).prefix();
	return NULL;
}
void MathStructure::setPrefix(Prefix *p) {
	if(isUnit()) {
		o_prefix = p;
	}
}
bool MathStructure::isPlural() const {
	return b_plural;
}
void MathStructure::setPlural(bool is_plural) {
	if(isUnit()) b_plural = is_plural;
}
void MathStructure::setFunction(MathFunction *f) {
	if(f) f->ref();
	if(o_function) o_function->unref();
	o_function = f;
}
void MathStructure::setUnit(Unit *u) {
	if(u) u->ref();
	if(o_unit) o_unit->unref();
	o_unit = u;
}
void MathStructure::setVariable(Variable *v) {
	if(v) v->ref();
	if(o_variable) o_variable->unref();
	o_variable = v;
}
MathFunction *MathStructure::function() const {
	return o_function;
}
Variable *MathStructure::variable() const {
	return o_variable;
}

bool MathStructure::isAddition() const {return m_type == STRUCT_ADDITION;}
bool MathStructure::isMultiplication() const {return m_type == STRUCT_MULTIPLICATION;}
bool MathStructure::isPower() const {return m_type == STRUCT_POWER;}
bool MathStructure::isSymbolic() const {return m_type == STRUCT_SYMBOLIC;}
bool MathStructure::isAborted() const {return m_type == STRUCT_ABORTED;}
bool MathStructure::isEmptySymbol() const {return m_type == STRUCT_SYMBOLIC && s_sym.empty();}
bool MathStructure::isVector() const {return m_type == STRUCT_VECTOR;}
bool MathStructure::isMatrix() const {
	if(m_type != STRUCT_VECTOR || SIZE < 1) return false;
	for(size_t i = 0; i < SIZE; i++) {
		if(!CHILD(i).isVector() || (i > 0 && CHILD(i).size() != CHILD(i - 1).size())) return false;
	}
	return true;
}
bool MathStructure::isFunction() const {return m_type == STRUCT_FUNCTION;}
bool MathStructure::isUnit() const {return m_type == STRUCT_UNIT;}
bool MathStructure::isUnit_exp() const {return m_type == STRUCT_UNIT || (m_type == STRUCT_POWER && CHILD(0).isUnit());}
bool MathStructure::isUnknown() const {return m_type == STRUCT_SYMBOLIC || (m_type == STRUCT_VARIABLE && o_variable && !o_variable->isKnown());}
bool MathStructure::isUnknown_exp() const {return isUnknown() || (m_type == STRUCT_POWER && CHILD(0).isUnknown());}
bool MathStructure::isNumber_exp() const {return m_type == STRUCT_NUMBER || (m_type == STRUCT_POWER && CHILD(0).isNumber());}
bool MathStructure::isVariable() const {return m_type == STRUCT_VARIABLE;}
bool MathStructure::isComparison() const {return m_type == STRUCT_COMPARISON;}
bool MathStructure::isLogicalAnd() const {return m_type == STRUCT_LOGICAL_AND;}
bool MathStructure::isLogicalOr() const {return m_type == STRUCT_LOGICAL_OR;}
bool MathStructure::isLogicalXor() const {return m_type == STRUCT_LOGICAL_XOR;}
bool MathStructure::isLogicalNot() const {return m_type == STRUCT_LOGICAL_NOT;}
bool MathStructure::isBitwiseAnd() const {return m_type == STRUCT_BITWISE_AND;}
bool MathStructure::isBitwiseOr() const {return m_type == STRUCT_BITWISE_OR;}
bool MathStructure::isBitwiseXor() const {return m_type == STRUCT_BITWISE_XOR;}
bool MathStructure::isBitwiseNot() const {return m_type == STRUCT_BITWISE_NOT;}
bool MathStructure::isInverse() const {return m_type == STRUCT_INVERSE;}
bool MathStructure::isDivision() const {return m_type == STRUCT_DIVISION;}
bool MathStructure::isNegate() const {return m_type == STRUCT_NEGATE;}
bool MathStructure::isInfinity() const {return m_type == STRUCT_NUMBER && o_number.isInfinite(true);}
bool MathStructure::isUndefined() const {return m_type == STRUCT_UNDEFINED || (m_type == STRUCT_NUMBER && o_number.isUndefined()) || (m_type == STRUCT_VARIABLE && o_variable == CALCULATOR->v_undef);}
bool MathStructure::isInteger() const {return m_type == STRUCT_NUMBER && o_number.isInteger();}
bool MathStructure::isNumber() const {return m_type == STRUCT_NUMBER;}
bool MathStructure::isZero() const {return m_type == STRUCT_NUMBER && o_number.isZero();}
bool MathStructure::isApproximatelyZero() const {return m_type == STRUCT_NUMBER && !o_number.isNonZero();}
bool MathStructure::isOne() const {return m_type == STRUCT_NUMBER && o_number.isOne();}
bool MathStructure::isMinusOne() const {return m_type == STRUCT_NUMBER && o_number.isMinusOne();}

bool MathStructure::hasNegativeSign() const {
	return (m_type == STRUCT_NUMBER && o_number.hasNegativeSign()) || m_type == STRUCT_NEGATE || (m_type == STRUCT_MULTIPLICATION && SIZE > 0 && CHILD(0).hasNegativeSign());
}

bool MathStructure::representsBoolean() const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isOne() || o_number.isZero();}
		case STRUCT_VARIABLE: {return o_variable->representsBoolean();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsBoolean()) || o_function->representsBoolean(*this);}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsBoolean()) return false;
			}
			return true;
		}
		case STRUCT_LOGICAL_NOT: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_COMPARISON: {return true;}
		default: {return false;}
	}
}

bool MathStructure::representsNumber(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return !o_number.includesInfinity();}
		case STRUCT_VARIABLE: {return o_variable->representsNumber(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNumber();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNumber(allow_units)) || o_function->representsNumber(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {}
		case STRUCT_POWER: {}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNumber(allow_units)) return false;
			}
			return true;
		}
		default: {return false;}
	}
}
bool MathStructure::representsInteger(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isInteger();}
		case STRUCT_VARIABLE: {return o_variable->representsInteger(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isInteger();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsInteger(allow_units)) || o_function->representsInteger(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsInteger(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsInteger(allow_units) && CHILD(1).representsInteger(false) && CHILD(1).representsPositive(false);
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonInteger(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return !o_number.isInteger();}
		case STRUCT_VARIABLE: {return o_variable->representsNonInteger(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonInteger(allow_units));}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {}
		case STRUCT_MULTIPLICATION: {
			return false;
		}
		case STRUCT_POWER: {
			return false;
		}
		default: {return false;}
	}
}
bool MathStructure::representsFraction(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isFraction();}
		case STRUCT_VARIABLE: {return o_variable->representsFraction(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsFraction(allow_units));}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {}
		case STRUCT_MULTIPLICATION: {
			return false;
		}
		case STRUCT_POWER: {
			return false;
		}
		default: {return false;}
	}
}
bool MathStructure::representsPositive(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isPositive();}
		case STRUCT_VARIABLE: {return o_variable->representsPositive(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isPositive();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsPositive(allow_units)) || o_function->representsPositive(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsPositive(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = true;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).representsPositive(allow_units) && CHILD(1).representsReal(false))
			|| (CHILD(0).representsNonZero(allow_units) && CHILD(0).representsReal(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsInteger(false));
		}
		default: {return false;}
	}
}
bool MathStructure::representsNegative(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNegative();}
		case STRUCT_VARIABLE: {return o_variable->representsNegative(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNegative();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNegative(allow_units)) || o_function->representsNegative(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNegative(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return CHILD(1).representsInteger(false) && CHILD(1).representsOdd(false) && CHILD(0).representsNegative(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonNegative(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNonNegative();}
		case STRUCT_VARIABLE: {return o_variable->representsNonNegative(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonNegative();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonNegative(allow_units)) || o_function->representsNonNegative(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonNegative(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = true;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsNonNegative(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).isZero() && CHILD(1).representsNonNegative(false))
			|| (CHILD(0).representsNonNegative(allow_units) && CHILD(1).representsReal(false))
			|| (CHILD(0).representsReal(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsInteger(false));
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonPositive(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNonPositive();}
		case STRUCT_VARIABLE: {return o_variable->representsNonPositive(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonPositive();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonPositive(allow_units)) || o_function->representsNonPositive(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonPositive(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).isZero() && CHILD(1).representsPositive(false)) || representsNegative(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsRational(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isRational();}
		case STRUCT_VARIABLE: {return o_variable->representsRational(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isRational();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsRational(allow_units)) || o_function->representsRational(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsRational(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsRational(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(1).representsInteger(false) && CHILD(0).representsRational(allow_units) && (CHILD(0).representsPositive(allow_units) || (CHILD(0).representsNegative(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsPositive(false)));
		}
		default: {return false;}
	}
}
bool MathStructure::representsReal(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isReal();}
		case STRUCT_VARIABLE: {return o_variable->representsReal(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isReal();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsReal(allow_units)) || o_function->representsReal(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsReal(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsReal(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return (CHILD(0).representsPositive(allow_units) && CHILD(1).representsReal(false)) 
			|| (CHILD(0).representsReal(allow_units) && CHILD(1).representsInteger(false) && (CHILD(1).representsPositive(false) || CHILD(0).representsNonZero(allow_units)));
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonComplex(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return !o_number.hasImaginaryPart();}
		case STRUCT_VARIABLE: {
			if(o_variable->isKnown()) return ((KnownVariable*) o_variable)->get().representsNonComplex(allow_units);
			return o_variable->representsNonComplex(allow_units);
		}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isReal();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonComplex(allow_units)) || o_function->representsReal(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonComplex(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonComplex(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return (CHILD(0).representsPositive(allow_units) && CHILD(1).representsReal(false)) 
			|| (CHILD(0).representsReal(allow_units) && CHILD(1).representsInteger(false));
		}
		default: {return false;}
	}
}
bool MathStructure::representsComplex(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.imaginaryPartIsNonZero();}
		case STRUCT_VARIABLE: {return o_variable->representsComplex(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isComplex();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsComplex(allow_units)) || o_function->representsComplex(*this, allow_units);}
		case STRUCT_ADDITION: {
			bool c = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsComplex(allow_units)) {
					if(c) return false;
					c = true;
				} else if(!CHILD(i).representsReal(allow_units)) {
					return false;
				}
			}
			return c;
		}
		case STRUCT_MULTIPLICATION: {
			bool c = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsComplex(allow_units)) {
					if(c) return false;
					c = true;
				} else if(!CHILD(i).representsReal(allow_units) || !CHILD(i).representsNonZero(allow_units)) {
					return false;
				}
			}
			return c;
		}
		case STRUCT_UNIT: {return false;}
		case STRUCT_POWER: {return CHILD(1).isNumber() && CHILD(1).number().isRational() && !CHILD(1).number().isInteger() && CHILD(0).representsNegative();}
		default: {return false;}
	}
}
bool MathStructure::representsNonZero(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNonZero();}
		case STRUCT_VARIABLE: {return o_variable->representsNonZero(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonZero();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonZero(allow_units)) || o_function->representsNonZero(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			bool neg = false, started = false;
			for(size_t i = 0; i < SIZE; i++) {
				if((!started || neg) && CHILD(i).representsNegative(allow_units)) {
					neg = true;
				} else if(neg || !CHILD(i).representsPositive(allow_units)) {
					return false;
				}
				started = true;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonZero(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsNonZero(allow_units) || (!CHILD(0).isApproximatelyZero() && CHILD(1).representsNonPositive());
		}
		default: {return false;}
	}
}
bool MathStructure::representsZero(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isZero();}
		case STRUCT_VARIABLE: {return o_variable->isKnown() && !o_variable->representsNonZero(allow_units) && ((KnownVariable*) o_variable)->get().representsZero();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsZero(allow_units));}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsZero(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsZero(allow_units)) {
					return true;
				}
			}
			return false;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsZero(allow_units) && CHILD(1).representsPositive(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsApproximatelyZero(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return !o_number.isNonZero();}
		case STRUCT_VARIABLE: {return o_variable->isKnown() && !o_variable->representsNonZero(allow_units) && ((KnownVariable*) o_variable)->get().representsApproximatelyZero();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsApproximatelyZero(allow_units));}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsApproximatelyZero(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsApproximatelyZero(allow_units)) {
					return true;
				}
			}
			return false;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsApproximatelyZero(allow_units) && CHILD(1).representsPositive(allow_units);
		}
		default: {return false;}
	}
}

bool MathStructure::representsEven(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isEven();}
		case STRUCT_VARIABLE: {return o_variable->representsEven(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsEven(allow_units)) || o_function->representsEven(*this, allow_units);}
		default: {return false;}
	}
}
bool MathStructure::representsOdd(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isOdd();}
		case STRUCT_VARIABLE: {return o_variable->representsOdd(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsOdd(allow_units)) || o_function->representsOdd(*this, allow_units);}
		default: {return false;}
	}
}
bool MathStructure::representsUndefined(bool include_childs, bool include_infinite, bool be_strict) const {
	switch(m_type) {
		case STRUCT_NUMBER: {
			if(include_infinite) {
				return o_number.includesInfinity();
			}
			return false;
		}
		case STRUCT_UNDEFINED: {return true;}
		case STRUCT_VARIABLE: {return o_variable->representsUndefined(include_childs, include_infinite, be_strict);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsUndefined(include_childs, include_infinite, be_strict)) || o_function->representsUndefined(*this);}
		case STRUCT_POWER: {
			if(be_strict) {
				if((!CHILD(0).representsNonZero(true) && !CHILD(1).representsNonNegative(false)) || (CHILD(0).isInfinity() && !CHILD(1).representsNonZero(true))) {
					return true;
				}
			} else {
				if((CHILD(0).representsZero(true) && CHILD(1).representsNegative(false)) || (CHILD(0).isInfinity() && CHILD(1).representsZero(true))) {
					return true;
				}
			}
		}
		default: {
			if(include_childs) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).representsUndefined(include_childs, include_infinite, be_strict)) return true;
				}
			}
			return false;
		}
	}
}
bool MathStructure::representsNonMatrix() const {
	switch(m_type) {
		case STRUCT_VECTOR: {return !isMatrix();}
		case STRUCT_POWER: {return CHILD(0).representsNonMatrix();}
		case STRUCT_VARIABLE: {return o_variable->representsNonMatrix();}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonMatrix();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonMatrix()) || o_function->representsNonMatrix(*this);}
		case STRUCT_INVERSE: {}
		case STRUCT_NEGATE: {}
		case STRUCT_DIVISION: {}
		case STRUCT_MULTIPLICATION: {}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonMatrix()) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_ABORTED: {
			return false;
		}
		default: {}
	}
	return true;
}
bool MathStructure::representsScalar() const {
	switch(m_type) {
		case STRUCT_VECTOR: {return false;}
		case STRUCT_POWER: {return CHILD(0).representsScalar();}
		case STRUCT_VARIABLE: {return o_variable->representsScalar();}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonMatrix();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsScalar()) || o_function->representsScalar(*this);}
		case STRUCT_INVERSE: {}
		case STRUCT_NEGATE: {}
		case STRUCT_DIVISION: {}
		case STRUCT_MULTIPLICATION: {}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsScalar()) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_ABORTED: {
			return false;
		}
		default: {}
	}
	return true;
}

void MathStructure::setApproximate(bool is_approx, bool recursive) {
	b_approx = is_approx;
	if(!b_approx) {
		i_precision = -1;
	}
	if(recursive) {
		if(m_type == STRUCT_NUMBER) {
			o_number.setApproximate(is_approx);
			if(i_precision < 0 || i_precision > o_number.precision()) i_precision = o_number.precision();
		}
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).setApproximate(is_approx, true);
		}
	}
}
bool MathStructure::isApproximate() const {
	return b_approx;
}

int MathStructure::precision() const {
	return i_precision;
}
void MathStructure::setPrecision(int prec, bool recursive) {
	i_precision = prec;
	if(i_precision > 0) b_approx = true;
	if(recursive) {
		if(m_type == STRUCT_NUMBER) {
			o_number.setPrecision(prec);
		}
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).setPrecision(prec, true);
		}
	}
}

void MathStructure::transform(StructureType mtype, const MathStructure &o) {
	MathStructure *struct_o = new MathStructure(o);
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(struct_o);
}
void MathStructure::transform(StructureType mtype, const Number &o) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(o);
}
void MathStructure::transform(StructureType mtype, int i) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(new MathStructure(i, 1, 0));
}
void MathStructure::transform(StructureType mtype, Unit *u) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(u);
}
void MathStructure::transform(StructureType mtype, Variable *v) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(v);
}
void MathStructure::transform(StructureType mtype, string sym) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(sym);
}
void MathStructure::transform_nocopy(StructureType mtype, MathStructure *o) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(o);
}
void MathStructure::transform(StructureType mtype) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
}
void MathStructure::transform(MathFunction *o) {
	transform(STRUCT_FUNCTION);
	setFunction(o);
}
void MathStructure::transform(ComparisonType ctype, const MathStructure &o) {
	MathStructure *struct_o = new MathStructure(o);
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = STRUCT_COMPARISON;
	ct_comp = ctype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(struct_o);
}
void transform(ComparisonType ctype, const MathStructure &o);
void MathStructure::add(const MathStructure &o, MathOperation op, bool append) {
	switch(op) {
		case OPERATION_ADD: {
			add(o, append);
			break;
		}
		case OPERATION_SUBTRACT: {
			subtract(o, append);
			break;
		}
		case OPERATION_MULTIPLY: {
			multiply(o, append);
			break;
		}
		case OPERATION_DIVIDE: {
			divide(o, append);
			break;
		}
		case OPERATION_RAISE: {
			raise(o);
			break;
		}
		case OPERATION_EXP10: {
			MathStructure *mstruct = new MathStructure(10, 1, 0);
			mstruct->raise(o);
			multiply_nocopy(mstruct, append);
			break;
		}
		case OPERATION_LOGICAL_AND: {
			if(m_type == STRUCT_LOGICAL_AND && append) {
				APPEND(o);
			} else {
				transform(STRUCT_LOGICAL_AND, o);
			}
			break;
		}
		case OPERATION_LOGICAL_OR: {
			if(m_type == STRUCT_LOGICAL_OR && append) {
				APPEND(o);
			} else {
				transform(STRUCT_LOGICAL_OR, o);
			}
			break;
		}
		case OPERATION_LOGICAL_XOR: {
			transform(STRUCT_LOGICAL_XOR, o);
			break;
		}
		case OPERATION_BITWISE_AND: {
			if(m_type == STRUCT_BITWISE_AND && append) {
				APPEND(o);
			} else {
				transform(STRUCT_BITWISE_AND, o);
			}
			break;
		}
		case OPERATION_BITWISE_OR: {
			if(m_type == STRUCT_BITWISE_OR && append) {
				APPEND(o);
			} else {
				transform(STRUCT_BITWISE_OR, o);
			}
			break;
		}
		case OPERATION_BITWISE_XOR: {
			transform(STRUCT_BITWISE_XOR, o);
			break;
		}
		case OPERATION_EQUALS: {}
		case OPERATION_NOT_EQUALS: {}
		case OPERATION_GREATER: {}
		case OPERATION_LESS: {}
		case OPERATION_EQUALS_GREATER: {}
		case OPERATION_EQUALS_LESS: {
			if(append && m_type == STRUCT_COMPARISON && append) {
				MathStructure *o2 = new MathStructure(CHILD(1));
				o2->add(o, op);
				transform_nocopy(STRUCT_LOGICAL_AND, o2);
			} else if(append && m_type == STRUCT_LOGICAL_AND && LAST.type() == STRUCT_COMPARISON) {
				MathStructure *o2 = new MathStructure(LAST[1]);
				o2->add(o, op);
				APPEND_POINTER(o2);
			} else {
				transform(STRUCT_COMPARISON, o);
				switch(op) {
					case OPERATION_EQUALS: {ct_comp = COMPARISON_EQUALS; break;}
					case OPERATION_NOT_EQUALS: {ct_comp = COMPARISON_NOT_EQUALS; break;}
					case OPERATION_GREATER: {ct_comp = COMPARISON_GREATER; break;}
					case OPERATION_LESS: {ct_comp = COMPARISON_LESS; break;}
					case OPERATION_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
					case OPERATION_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_LESS; break;}
					default: {}
				}
			}
			break;
		}
		default: {
		}
	}
}
void MathStructure::add(const MathStructure &o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND(o);
	} else {
		transform(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract(const MathStructure &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(const MathStructure &o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND(o);
	} else {
		transform(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide(const MathStructure &o, bool append) {
//	transform(STRUCT_DIVISION, o);
	MathStructure *o2 = new MathStructure(o);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(const MathStructure &o) {
	transform(STRUCT_POWER, o);
}
void MathStructure::add(const Number &o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(o);
	} else {
		transform(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract(const Number &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(const Number &o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(o);
	} else {
		transform(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide(const Number &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(const Number &o) {
	transform(STRUCT_POWER, o);
}
void MathStructure::add(int i, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_POINTER(new MathStructure(i, 1, 0));
	} else {
		transform(STRUCT_ADDITION, i);
	}
}
void MathStructure::subtract(int i, bool append) {
	MathStructure *o2 = new MathStructure(i, 1, 0);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(int i, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_POINTER(new MathStructure(i, 1, 0));
	} else {
		transform(STRUCT_MULTIPLICATION, i);
	}
}
void MathStructure::divide(int i, bool append) {
	MathStructure *o2 = new MathStructure(i, 1, 0);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(int i) {
	transform(STRUCT_POWER, i);
}
void MathStructure::add(Variable *v, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(v);
	} else {
		transform(STRUCT_ADDITION, v);
	}
}
void MathStructure::subtract(Variable *v, bool append) {
	MathStructure *o2 = new MathStructure(v);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(Variable *v, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(v);
	} else {
		transform(STRUCT_MULTIPLICATION, v);
	}
}
void MathStructure::divide(Variable *v, bool append) {
	MathStructure *o2 = new MathStructure(v);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(Variable *v) {
	transform(STRUCT_POWER, v);
}
void MathStructure::add(Unit *u, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(u);
	} else {
		transform(STRUCT_ADDITION, u);
	}
}
void MathStructure::subtract(Unit *u, bool append) {
	MathStructure *o2 = new MathStructure(u);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(Unit *u, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(u);
	} else {
		transform(STRUCT_MULTIPLICATION, u);
	}
}
void MathStructure::divide(Unit *u, bool append) {
	MathStructure *o2 = new MathStructure(u);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(Unit *u) {
	transform(STRUCT_POWER, u);
}
void MathStructure::add(string sym, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(sym);
	} else {
		transform(STRUCT_ADDITION, sym);
	}
}
void MathStructure::subtract(string sym, bool append) {
	MathStructure *o2 = new MathStructure(sym);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(string sym, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(sym);
	} else {
		transform(STRUCT_MULTIPLICATION, sym);
	}
}
void MathStructure::divide(string sym, bool append) {
	MathStructure *o2 = new MathStructure(sym);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(string sym) {
	transform(STRUCT_POWER, sym);
}
void MathStructure::add_nocopy(MathStructure *o, MathOperation op, bool append) {
	switch(op) {
		case OPERATION_ADD: {
			add_nocopy(o, append);
			break;
		}
		case OPERATION_SUBTRACT: {
			subtract_nocopy(o, append);
			break;
		}
		case OPERATION_MULTIPLY: {
			multiply_nocopy(o, append);
			break;
		}
		case OPERATION_DIVIDE: {
			divide_nocopy(o, append);
			break;
		}
		case OPERATION_RAISE: {
			raise_nocopy(o);
			break;
		}
		case OPERATION_EXP10: {
			MathStructure *mstruct = new MathStructure(10, 1, 0);
			mstruct->raise_nocopy(o);
			multiply_nocopy(mstruct, append);
			break;
		}
		case OPERATION_LOGICAL_AND: {
			if(m_type == STRUCT_LOGICAL_AND && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_LOGICAL_AND, o);
			}
			break;
		}
		case OPERATION_LOGICAL_OR: {
			if(m_type == STRUCT_LOGICAL_OR && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_LOGICAL_OR, o);
			}
			break;
		}
		case OPERATION_LOGICAL_XOR: {
			transform_nocopy(STRUCT_LOGICAL_XOR, o);
			break;
		}
		case OPERATION_BITWISE_AND: {
			if(m_type == STRUCT_BITWISE_AND && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_BITWISE_AND, o);
			}
			break;
		}
		case OPERATION_BITWISE_OR: {
			if(m_type == STRUCT_BITWISE_OR && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_BITWISE_OR, o);
			}
			break;
		}
		case OPERATION_BITWISE_XOR: {
			transform_nocopy(STRUCT_BITWISE_XOR, o);
			break;
		}
		case OPERATION_EQUALS: {}
		case OPERATION_NOT_EQUALS: {}
		case OPERATION_GREATER: {}
		case OPERATION_LESS: {}
		case OPERATION_EQUALS_GREATER: {}
		case OPERATION_EQUALS_LESS: {
			if(append && m_type == STRUCT_COMPARISON && append) {
				MathStructure *o2 = new MathStructure(CHILD(1));
				o2->add_nocopy(o, op);
				transform_nocopy(STRUCT_LOGICAL_AND, o2);
			} else if(append && m_type == STRUCT_LOGICAL_AND && LAST.type() == STRUCT_COMPARISON) {
				MathStructure *o2 = new MathStructure(LAST[1]);
				o2->add_nocopy(o, op);
				APPEND_POINTER(o2);
			} else {
				transform_nocopy(STRUCT_COMPARISON, o);
				switch(op) {
					case OPERATION_EQUALS: {ct_comp = COMPARISON_EQUALS; break;}
					case OPERATION_NOT_EQUALS: {ct_comp = COMPARISON_NOT_EQUALS; break;}
					case OPERATION_GREATER: {ct_comp = COMPARISON_GREATER; break;}
					case OPERATION_LESS: {ct_comp = COMPARISON_LESS; break;}
					case OPERATION_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
					case OPERATION_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_LESS; break;}
					default: {}
				}
			}
			break;
		}
		default: {
		}
	}
}
void MathStructure::add_nocopy(MathStructure *o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_POINTER(o);
	} else {
		transform_nocopy(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract_nocopy(MathStructure *o, bool append) {
	o->negate();
	add_nocopy(o, append);
}
void MathStructure::multiply_nocopy(MathStructure *o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_POINTER(o);
	} else {
		transform_nocopy(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide_nocopy(MathStructure *o, bool append) {
	o->inverse();
	multiply_nocopy(o, append);
}
void MathStructure::raise_nocopy(MathStructure *o) {
	transform_nocopy(STRUCT_POWER, o);
}
void MathStructure::negate() {
	//transform(STRUCT_NEGATE);
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = STRUCT_MULTIPLICATION;
	APPEND(m_minus_one);
	APPEND_POINTER(struct_this);
}
void MathStructure::inverse() {
	//transform(STRUCT_INVERSE);
	raise(m_minus_one);
}
void MathStructure::setLogicalNot() {
	transform(STRUCT_LOGICAL_NOT);
}		
void MathStructure::setBitwiseNot() {
	transform(STRUCT_BITWISE_NOT);
}		

bool MathStructure::equals(const MathStructure &o, bool allow_interval) const {
	if(m_type != o.type()) return false;
	if(SIZE != o.size()) return false;
	switch(m_type) {
		case STRUCT_UNDEFINED: {return true;}
		case STRUCT_SYMBOLIC: {return s_sym == o.symbol();}
		case STRUCT_NUMBER: {return o_number.equals(o.number(), allow_interval);}
		case STRUCT_VARIABLE: {return o_variable == o.variable();}
		case STRUCT_UNIT: {
			Prefix *p1 = (o_prefix == NULL || o_prefix == CALCULATOR->decimal_null_prefix || o_prefix == CALCULATOR->binary_null_prefix) ? NULL : o_prefix;
			Prefix *p2 = (o.prefix() == NULL || o.prefix() == CALCULATOR->decimal_null_prefix || o.prefix() == CALCULATOR->binary_null_prefix) ? NULL : o.prefix();
			return o_unit == o.unit() && p1 == p2;
		}
		case STRUCT_COMPARISON: {if(ct_comp != o.comparisonType()) return false; break;}
		case STRUCT_FUNCTION: {if(o_function != o.function()) return false; break;}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_LOGICAL_AND: {
			if(SIZE < 1) return false;
			if(SIZE == 2) {
				return (CHILD(0) == o[0] && CHILD(1) == o[1]) || (CHILD(0) == o[1] && CHILD(1) == o[0]);
			}
			vector<size_t> i2taken;
			for(size_t i = 0; i < SIZE; i++) {
				bool b = false, b2 = false;
				for(size_t i2 = 0; i2 < o.size(); i2++) {
					if(CHILD(i).equals(o[i2], allow_interval)) {
						b2 = true;
						for(size_t i3 = 0; i3 < i2taken.size(); i3++) {
							if(i2taken[i3] == i2) {
								b2 = false;
							}
						}
						if(b2) {
							b = true;
							i2taken.push_back(i2);
							break;
						}
					}
				}
				if(!b) return false;
			}
			return true;
		}
		default: {}
	}
	if(SIZE < 1) return false;
	for(size_t i = 0; i < SIZE; i++) {
		if(!CHILD(i).equals(o[i], allow_interval)) return false;
	}
	return true;
}
bool MathStructure::equals(const Number &o, bool allow_interval) const {
	if(m_type != STRUCT_NUMBER) return false;
	return o_number.equals(o, allow_interval);
}
bool MathStructure::equals(int i) const {
	if(m_type != STRUCT_NUMBER) return false;
	return o_number.equals(Number(i, 1, 0));
}
bool MathStructure::equals(Unit *u) const {
	if(m_type != STRUCT_UNIT) return false;
	return o_unit == u;
}
bool MathStructure::equals(Variable *v) const {
	if(m_type != STRUCT_VARIABLE) return false;
	return o_variable == v;
}
bool MathStructure::equals(string sym) const {
	if(m_type != STRUCT_SYMBOLIC) return false;
	return s_sym == sym;
}

int compare_check_incompability(MathStructure *mtest) {
	int incomp = 0;
	int unit_term_count = 0;
	int not_unit_term_count = 0;
	int compat_count = 0;
	bool b_not_number = false;
	for(size_t i = 0; i < mtest->size(); i++) {					
		if((*mtest)[i].containsType(STRUCT_UNIT, false, true, true) > 0) {
			unit_term_count++;
			for(size_t i2 = i + 1; i2 < mtest->size(); i2++) {
				int b_test = (*mtest)[i].isUnitCompatible((*mtest)[i2]);
				if(b_test == 0) {
					incomp = 1;
				} else if(b_test > 0) {
					compat_count++;
				}
			}
			if(!b_not_number && !(*mtest)[i].representsNumber(true)) {
				b_not_number = true;
			}
		} else if((*mtest)[i].containsRepresentativeOfType(STRUCT_UNIT, true, true) == 0) {
			not_unit_term_count++;
		} else if(!b_not_number && !(*mtest)[i].representsNumber(true)) {
			b_not_number = true;
		}
	}
	if(b_not_number && unit_term_count > 0) {
		incomp = -1;
	} else if(unit_term_count > 0) {
		if((long int) mtest->size() - (unit_term_count + not_unit_term_count) >= unit_term_count - compat_count + (not_unit_term_count > 0)) {
			incomp = -1;
		} else if(not_unit_term_count > 0) {
			incomp = 1;
		}
	}
	return incomp;
}

bool MathStructure::mergeInterval(const MathStructure &o, bool set_to_overlap) {
	if(isNumber() && o.isNumber()) {
		return o_number.mergeInterval(o.number(), set_to_overlap);
	}
	if(equals(o)) return true;
	if(isMultiplication() && SIZE > 1 && CHILD(0).isNumber()) {
		if(o.isMultiplication() && o.size() > 1) {
			if(SIZE == o.size() + (o[0].isNumber() ? 0 : 1)) {
				bool b = true;
				for(size_t i = 1; i < SIZE; i++) {
					if(!CHILD(i).equals(o[0].isNumber() ? o[i] : o[i - 1]) || !CHILD(i).representsNonNegative(true)) {
						b = false;
						break;
					}
				}
				if(b && o[0].isNumber()) {
					if(!CHILD(0).number().mergeInterval(o[0].number(), set_to_overlap)) return false;
				} else if(b) {
					if(!CHILD(0).number().mergeInterval(nr_one, set_to_overlap)) return false;
				}
				CHILD(0).numberUpdated();
				CHILD_UPDATED(0);
				return true;
			}
		} else if(SIZE == 2 && o.equals(CHILD(1)) && o.representsNonNegative(true)) {
			if(!CHILD(0).number().mergeInterval(nr_one, set_to_overlap)) return false;
			CHILD(0).numberUpdated();
			CHILD_UPDATED(0);
			return true;
		}
	} else if(o.isMultiplication() && o.size() == 2 && o[0].isNumber() && equals(o[1]) && representsNonNegative(true)) {
		Number nr(1, 1);
		if(!nr.mergeInterval(o[0].number(), set_to_overlap)) return false;
		transform(STRUCT_MULTIPLICATION);
		PREPEND(nr);
		return true;
	}
	return false;
}

ComparisonResult MathStructure::compare(const MathStructure &o) const {
	if(isNumber() && o.isNumber()) {
		return o_number.compare(o.number());
	}
	if(equals(o)) return COMPARISON_RESULT_EQUAL;
	if(isMultiplication() && SIZE > 1 && CHILD(0).isNumber()) {
		if(o.isMultiplication() && o.size() > 1) {
			if(SIZE == o.size() + (o[0].isNumber() ? 0 : 1)) {
				bool b = true;
				for(size_t i = 1; i < SIZE; i++) {
					if(!CHILD(i).equals(o[0].isNumber() ? o[i] : o[i - 1]) || !CHILD(i).representsNonNegative(true)) {
						b = false;
						break;
					}
				}
				if(b && o[0].isNumber()) return CHILD(0).number().compare(o[0].number());
				else if(b) return CHILD(0).number().compare(nr_one);
			}
		} else if(SIZE == 2 && o.equals(CHILD(1)) && o.representsNonNegative(true)) {
			return CHILD(0).number().compare(nr_one);
		}
	} else if(o.isMultiplication() && o.size() == 2 && o[0].isNumber() && equals(o[1]) && representsNonNegative(true)) {
		return nr_one.compare(o[0].number());
	}
	if(o.representsReal(true) && representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	if(representsReal(true) && o.representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	CALCULATOR->beginTemporaryStopMessages();
	MathStructure mtest(*this);
	mtest -= o;
	EvaluationOptions eo = default_evaluation_options;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	mtest.calculatesub(eo, eo);
	CALCULATOR->endTemporaryStopMessages();
	int incomp = 0;
	if(mtest.isAddition()) {
		incomp = compare_check_incompability(&mtest);
	}
	if(incomp > 0) return COMPARISON_RESULT_NOT_EQUAL;
	if(incomp == 0) {
		if(mtest.representsZero(true)) return COMPARISON_RESULT_EQUAL;
		if(mtest.representsPositive(true)) return COMPARISON_RESULT_LESS;
		if(mtest.representsNegative(true)) return COMPARISON_RESULT_GREATER;
		if(mtest.representsNonZero(true)) return COMPARISON_RESULT_NOT_EQUAL;
		if(mtest.representsNonPositive(true)) return COMPARISON_RESULT_EQUAL_OR_LESS;
		if(mtest.representsNonNegative(true)) return COMPARISON_RESULT_EQUAL_OR_GREATER;
	} else {
		bool a_pos = representsPositive(true);
		bool a_nneg = a_pos || representsNonNegative(true);
		bool a_neg = !a_nneg && representsNegative(true);
		bool a_npos = !a_pos && (a_neg || representsNonPositive(true));
		bool b_pos = o.representsPositive(true);
		bool b_nneg = b_pos || o.representsNonNegative(true);
		bool b_neg = !b_nneg && o.representsNegative(true);
		bool b_npos = !b_pos && (b_neg || o.representsNonPositive(true));
		if(a_pos && b_npos) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_npos && b_pos) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_nneg && b_neg) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_neg && b_nneg) return COMPARISON_RESULT_NOT_EQUAL;
	}
	return COMPARISON_RESULT_UNKNOWN;
}
ComparisonResult MathStructure::compareApproximately(const MathStructure &o, const EvaluationOptions &eo2) const {
	if(isNumber() && o.isNumber()) {
		return o_number.compareApproximately(o.number());
	}
	if(equals(o)) return COMPARISON_RESULT_EQUAL;
	if(isMultiplication() && SIZE > 1 && CHILD(0).isNumber()) {
		if(o.isMultiplication() && o.size() > 1) {
			if(SIZE == o.size() + (o[0].isNumber() ? 0 : 1)) {
				bool b = true;
				for(size_t i = 1; i < SIZE; i++) {
					if(!CHILD(i).equals(o[0].isNumber() ? o[i] : o[i - 1]) || !CHILD(i).representsNonNegative(true)) {
						b = false;
						break;
					}
				}
				if(b && o[0].isNumber()) return CHILD(0).number().compareApproximately(o[0].number());
				else if(b) return CHILD(0).number().compareApproximately(nr_one);
			}
		} else if(SIZE == 2 && o.equals(CHILD(1)) && o.representsNonNegative(true)) {
			return CHILD(0).number().compareApproximately(nr_one);
		}
	} else if(o.isMultiplication() && o.size() == 2 && o[0].isNumber() && equals(o[1]) && representsNonNegative(true)) {
		return nr_one.compareApproximately(o[0].number());
	}
	if(o.representsZero(true) && representsZero(true)) return COMPARISON_RESULT_EQUAL;
	if(o.representsReal(true) && representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	if(representsReal(true) && o.representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	CALCULATOR->beginTemporaryStopMessages();
	MathStructure mtest(*this), mtest2(o);
	EvaluationOptions eo = eo2;
	eo.expand = true;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	mtest.calculatesub(eo, eo);
	mtest2.calculatesub(eo, eo);
	CALCULATOR->endTemporaryStopMessages();
	if(mtest.equals(mtest2)) return COMPARISON_RESULT_EQUAL;
	if(mtest.representsZero(true) && mtest2.representsZero(true)) return COMPARISON_RESULT_EQUAL;
	if(mtest.isNumber() && mtest2.isNumber()) {
		if(mtest2.isApproximate() && o.isAddition() && o.size() > 1 && mtest.isZero() && !mtest2.isZero()) {
			CALCULATOR->beginTemporaryStopMessages();
			mtest = *this;
			mtest.subtract(o[0]);
			mtest2 = o;
			mtest2.delChild(1, true);
			mtest.calculatesub(eo, eo);
			mtest2.calculatesub(eo, eo);
			CALCULATOR->endTemporaryStopMessages();
			if(mtest.isNumber() && mtest2.isNumber()) return mtest.number().compareApproximately(mtest2.number());
		} else if(mtest.isApproximate() && isAddition() && SIZE > 1 && mtest2.isZero() && !mtest.isZero()) {
			CALCULATOR->beginTemporaryStopMessages();
			mtest2 = o;
			mtest2.subtract(CHILD(0));
			mtest = *this;
			mtest.delChild(1, true);
			mtest.calculatesub(eo, eo);
			mtest2.calculatesub(eo, eo);
			CALCULATOR->endTemporaryStopMessages();
			if(mtest.isNumber() && mtest2.isNumber()) return mtest.number().compareApproximately(mtest2.number());
		} else {
			return mtest.number().compareApproximately(o.number());
		}
	}
	CALCULATOR->beginTemporaryStopMessages();
	mtest -= o;
	mtest.calculatesub(eo, eo);
	CALCULATOR->endTemporaryStopMessages();
	if(mtest.representsZero(true)) return COMPARISON_RESULT_EQUAL;
	int incomp = 0;
	if(mtest.isAddition()) {
		incomp = compare_check_incompability(&mtest);
	}
	if(incomp > 0) return COMPARISON_RESULT_NOT_EQUAL;
	if(incomp == 0) {
		if(mtest.isZero()) return COMPARISON_RESULT_EQUAL;
		if(mtest.representsPositive(true)) return COMPARISON_RESULT_LESS;
		if(mtest.representsNegative(true)) return COMPARISON_RESULT_GREATER;
		if(mtest.representsNonZero(true)) return COMPARISON_RESULT_NOT_EQUAL;
		if(mtest.representsNonPositive(true)) return COMPARISON_RESULT_EQUAL_OR_LESS;
		if(mtest.representsNonNegative(true)) return COMPARISON_RESULT_EQUAL_OR_GREATER;
	} else {
		bool a_pos = representsPositive(true);
		bool a_nneg = a_pos || representsNonNegative(true);
		bool a_neg = !a_nneg && representsNegative(true);
		bool a_npos = !a_pos && (a_neg || representsNonPositive(true));
		bool b_pos = o.representsPositive(true);
		bool b_nneg = b_pos || o.representsNonNegative(true);
		bool b_neg = !b_nneg && o.representsNegative(true);
		bool b_npos = !b_pos && (b_neg || o.representsNonPositive(true));
		if(a_pos && b_npos) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_npos && b_pos) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_nneg && b_neg) return COMPARISON_RESULT_NOT_EQUAL;
		if(a_neg && b_nneg) return COMPARISON_RESULT_NOT_EQUAL;
	}
	return COMPARISON_RESULT_UNKNOWN;
}

int MathStructure::merge_addition(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this, size_t index_mstruct, bool reversed) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.add(mstruct.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate())) {
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	if(isZero()) {
		if(mparent) {
			mparent->swapChildren(index_this + 1, index_mstruct + 1);
		} else {
			set_nocopy(mstruct, true);
		}
		return 3;
	}
	if(mstruct.isZero()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite()) {
		if(mstruct.representsNumber(false)) {
			MERGE_APPROX_AND_PREC(mstruct)
			return 2;
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite()) {
		if(representsNumber(false)) {			
			if(mparent) {
				mparent->swapChildren(index_this + 1, index_mstruct + 1);
			} else {
				clear(true);
				o_number = mstruct.number();
				MERGE_APPROX_AND_PREC(mstruct)
			}
			return 3;
		}
	}
	if(representsUndefined() || mstruct.representsUndefined()) return -1;
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_ADDITION: {return 0;}
				case STRUCT_VECTOR: {
					if(SIZE == mstruct.size()) {
						for(size_t i = 0; i < SIZE; i++) {
							CHILD(i).calculateAdd(mstruct[i], eo, this, i);
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}				
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_ADDITION: {
			switch(mstruct.type()) {
				case STRUCT_ADDITION: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						if(reversed) {
							INSERT_REF(&mstruct[i], i)
							calculateAddIndex(i, eo, false);
						} else {
							APPEND_REF(&mstruct[i]);
							calculateAddLast(eo, false);
						}
					}
					MERGE_APPROX_AND_PREC(mstruct)
					if(SIZE == 1) {
						setToChild(1, false, mparent, index_this + 1);
					} else if(SIZE == 0) {
						clear(true);
					} else {
						evalSort();
					}
					return 1;
				}
				default: {
					MERGE_APPROX_AND_PREC(mstruct)
					if(reversed) {
						PREPEND_REF(&mstruct);
						calculateAddIndex(0, eo, true, mparent, index_this);
					} else {
						APPEND_REF(&mstruct);
						calculateAddLast(eo, true, mparent, index_this);
					}
					return 1;
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {return -1;}
				case STRUCT_ADDITION: {
					return 0;
				}
				case STRUCT_MULTIPLICATION: {
					size_t i1 = 0, i2 = 0;
					bool b = true;
					if(CHILD(0).isNumber()) i1 = 1;
					if(mstruct[0].isNumber()) i2 = 1;
					if(SIZE - i1 == mstruct.size() - i2) {
						for(size_t i = i1; i < SIZE; i++) {
							if(CHILD(i) != mstruct[i + i2 - i1]) {
								b = false;
								break;
							}
						}
						if(b) {
							if(i1 == 0) {
								PREPEND(m_one);
							}
							if(i2 == 0) {
								CHILD(0).number()++;
							} else {
								CHILD(0).number() += mstruct[0].number();
							}
							MERGE_APPROX_AND_PREC(mstruct)
							calculateMultiplyIndex(0, eo, true, mparent, index_this);
							return 1;
						}
					}
					if(eo.protected_function != CALCULATOR->f_signum) {
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isFunction() && CHILD(i).function() == CALCULATOR->f_signum && CHILD(i).size() == 2 && CHILD(i)[0].isAddition() && CHILD(i)[0].size() == 2 && CHILD(i)[0].representsReal(true)) {
								for(size_t im = 0; im < mstruct.size(); im++) {
									if(mstruct[im] == CHILD(i)) {
										MathStructure m1(*this), m2(mstruct);
										m1.delChild(i + 1, true);
										m2.delChild(im + 1, true);
										if((m1 == CHILD(i)[0][0] && m2 == CHILD(i)[0][1]) || (m2 == CHILD(i)[0][0] && m1 == CHILD(i)[0][1])) {
											SET_CHILD_MAP(i)
											setFunction(CALCULATOR->f_abs);
											ERASE(1)
											MERGE_APPROX_AND_PREC(mstruct)
											return 1;
										}
									}
								}
							}
						}
					}
					if(!eo.combine_divisions) break;
					b = true; size_t divs = 0;
					for(; b && i1 < SIZE; i1++) {
						if(CHILD(i1).isPower() && CHILD(i1)[1].hasNegativeSign()) {
							divs++;
							b = false;
							for(; i2 < mstruct.size(); i2++) {
								if(mstruct[i2].isPower() && mstruct[i2][1].hasNegativeSign()) {
									if(mstruct[i2] == CHILD(i1)) {
										b = true;
									}
									i2++;
									break;
								}
							}
						}
					}
					if(b && divs > 0) {
						for(; i2 < mstruct.size(); i2++) {
							if(mstruct[i2].isPower() && mstruct[i2][1].hasNegativeSign()) {
								b = false;
								break;
							}
						}
					}
					if(b && divs > 0) {
						if(SIZE - divs == 0) {
							if(mstruct.size() - divs == 0) {
								calculateMultiply(nr_two, eo);
							} else if(mstruct.size() - divs == 1) {
								PREPEND(m_one);
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										mstruct[i].ref();
										CHILD(0).add_nocopy(&mstruct[i], true);
										CHILD(0).calculateAddLast(eo, true, this, 0);
										break;
									}
								}
								calculateMultiplyIndex(0, eo, true, mparent, index_this);
							} else {								
								for(size_t i = 0; i < mstruct.size();) {
									if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
										mstruct.delChild(i + 1);
									} else {
										i++;
									}
								}
								PREPEND(m_one);
								mstruct.ref();
								CHILD(0).add_nocopy(&mstruct, true);
								CHILD(0).calculateAddLast(eo, true, this, 0);
								calculateMultiplyIndex(0, eo, true, mparent, index_this);
							}
						} else if(SIZE - divs == 1) {
							size_t index = 0;
							for(; index < SIZE; index++) {
								if(!CHILD(index).isPower() || !CHILD(index)[1].hasNegativeSign()) {
									break;
								}
							}
							if(mstruct.size() - divs == 0) {
								if(IS_REAL(CHILD(index))) {
									CHILD(index).number()++;
								} else {
									CHILD(index).calculateAdd(m_one, eo, this, index);
								}
								calculateMultiplyIndex(index, eo, true, mparent, index_this);
							} else if(mstruct.size() - divs == 1) {
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										mstruct[i].ref();
										CHILD(index).add_nocopy(&mstruct[i], true);
										CHILD(index).calculateAddLast(eo, true, this, index);
										break;
									}
								}
								calculateMultiplyIndex(index, eo, true, mparent, index_this);
							} else {
								for(size_t i = 0; i < mstruct.size();) {
									if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
										mstruct.delChild(i + 1);
									} else {
										i++;
									}
								}
								mstruct.ref();
								CHILD(index).add_nocopy(&mstruct, true);
								CHILD(index).calculateAddLast(eo, true, this, index);
								calculateMultiplyIndex(index, eo, true, mparent, index_this);
							}
						} else {
							for(size_t i = 0; i < SIZE;) {
								if(CHILD(i).isPower() && CHILD(i)[1].hasNegativeSign()) {
									ERASE(i);
								} else {
									i++;
								}
							}
							if(mstruct.size() - divs == 0) {
								calculateAdd(m_one, eo);
							} else if(mstruct.size() - divs == 1) {
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										mstruct[i].ref();
										add_nocopy(&mstruct[i], true);
										calculateAddLast(eo);
										break;
									}
								}
							} else {
								MathStructure *mstruct2 = new MathStructure();
								mstruct2->setType(STRUCT_MULTIPLICATION);
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										mstruct[i].ref();
										mstruct2->addChild_nocopy(&mstruct[i]);
									}
								}
								add_nocopy(mstruct2, true);
								calculateAddLast(eo, true, mparent, index_this);
							}
							for(size_t i = 0; i < mstruct.size(); i++) {
								if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
									mstruct[i].ref();
									multiply_nocopy(&mstruct[i], true);
									calculateMultiplyLast(eo);
								}
							}
						}
						return 1;
					}
					break;
				}
				case STRUCT_POWER: {
					if(eo.combine_divisions && mstruct[1].hasNegativeSign()) {
						bool b = false;
						size_t index = 0;
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isPower() && CHILD(i)[1].hasNegativeSign()) {
								if(b) {
									b = false;
									break;
								}
								if(mstruct == CHILD(i)) {
									index = i;
									b = true;
								}
								if(!b) break;
							}
						}
						if(b) {
							if(SIZE == 2) {
								if(index == 0) setToChild(2, true);
								else setToChild(1, true);
							} else {
								ERASE(index);
							}
							calculateAdd(m_one, eo);
							mstruct.ref();
							multiply_nocopy(&mstruct, false);
							calculateMultiplyLast(eo, true, mparent, index_this);
							return 1;
						}
					}
				}
				default: {
					if(SIZE == 2 && CHILD(0).isNumber() && CHILD(1) == mstruct) {
						CHILD(0).number()++;
						MERGE_APPROX_AND_PREC(mstruct)
						calculateMultiplyIndex(0, eo, true, mparent, index_this);
						return 1;
					}
					if(mstruct.type() == STRUCT_FUNCTION && mstruct.function() == CALCULATOR->f_signum && eo.protected_function != CALCULATOR->f_signum) {
						return 0;
					}
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(mstruct.type() == STRUCT_POWER) {
				if(CHILD(0).isFunction() && mstruct[0].isFunction() && ((CHILD(0).function() == CALCULATOR->f_cos && mstruct[0].function() == CALCULATOR->f_sin) || (CHILD(0).function() == CALCULATOR->f_sin && mstruct[0].function() == CALCULATOR->f_cos)) && CHILD(0).size() == 1 && mstruct[0].size() == 1 && CHILD(1).isNumber() && CHILD(1).number().isTwo() && mstruct[1] == CHILD(1) && CHILD(0)[0] == mstruct[0][0]) {
					// cos(x)^2+sin(x)^2=1
					set(1, 1, 0, true);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_signum && mstruct.isMultiplication() && eo.protected_function != CALCULATOR->f_signum) {
				if(SIZE == 2 && CHILD(0).isAddition() && CHILD(0).size() == 2 && (CHILD(0)[0].isOne() || CHILD(0)[1].isOne()) && CHILD(0).representsReal(true)) {
					for(size_t im = 0; im < mstruct.size(); im++) {
						if(mstruct[im] == *this) {
							MathStructure m2(mstruct);
							m2.delChild(im + 1, true);
							if((CHILD(0)[0].isOne() && m2 == CHILD(0)[1]) || (CHILD(0)[1].isOne() && m2 == CHILD(0)[0])) {
								setFunction(CALCULATOR->f_abs);
								ERASE(1)
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							}
						}
					}
				}
			}
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {return -1;}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {
					return 0;
				}
				default: {
					if(equals(mstruct)) {
						multiply_nocopy(new MathStructure(2, 1, 0), true);
						MERGE_APPROX_AND_PREC(mstruct)
						calculateMultiplyLast(eo, true, mparent, index_this);
						return 1;
					}
				}
			}	
		}		
	}
	return -1;
}

bool reducable(const MathStructure &mnum, const MathStructure &mden, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {}
		case STRUCT_ADDITION: {
			break;
		}
		default: {
			bool reduce = true;
			for(size_t i = 0; i < mden.size() && reduce; i++) {
				switch(mden[i].type()) {
					case STRUCT_MULTIPLICATION: {
						reduce = false;
						for(size_t i2 = 0; i2 < mden[i].size(); i2++) {
							if(mnum == mden[i][i2]) {
								reduce = true;
								if(!nr.isOne() && !nr.isFraction()) nr.set(1, 1, 0);
								break;
							} else if(mden[i][i2].isPower() && mden[i][i2][1].isNumber() && mden[i][i2][1].number().isReal() && mnum == mden[i][i2][0]) {
								if(!mden[i][i2][1].number().isPositive()) {
									break;
								}
								if(mden[i][i2][1].number().isLessThan(nr)) nr = mden[i][i2][1].number();
								reduce = true;
								break;
							}
						}
						break;
					}
					case STRUCT_POWER: {
						if(mden[i][1].isNumber() && mden[i][1].number().isReal() && mnum == mden[i][0]) {
							if(!mden[i][1].number().isPositive()) {
								reduce = false;
								break;
							}
							if(mden[i][1].number().isLessThan(nr)) nr = mden[i][1].number();
							break;
						}
					}
					default: {
						if(mnum != mden[i]) {
							reduce = false;
							break;
						}
						if(!nr.isOne() && !nr.isFraction()) nr.set(1, 1, 0);
					}
				}
			}
			return reduce;
		}
	}
	return false;
}
void reduce(const MathStructure &mnum, MathStructure &mden, Number &nr, const EvaluationOptions &eo) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {}
		case STRUCT_ADDITION: {
			break;
		}
		default: {
			for(size_t i = 0; i < mden.size(); i++) {
				switch(mden[i].type()) {
					case STRUCT_MULTIPLICATION: {
						for(size_t i2 = 0; i2 < mden[i].size(); i2++) {
							if(mden[i][i2] == mnum) {
								if(!nr.isOne()) {
									MathStructure *mexp = new MathStructure(1, 1, 0);
									mexp->number() -= nr;
									mden[i][i2].raise_nocopy(mexp);
									mden[i][i2].calculateRaiseExponent(eo);
								} else {
									if(mden[i].size() == 1) {
										mden[i].set(m_one);
									} else {
										mden[i].delChild(i2 + 1);
										if(mden[i].size() == 1) {
											mden[i].setToChild(1, true, &mden, i + 1);
										}
									}
								}
								break;
							} else if(mden[i][i2].isPower() && mden[i][i2][1].isNumber() && mden[i][i2][1].number().isReal() && mnum.equals(mden[i][i2][0])) {
								mden[i][i2][1].number() -= nr;
								if(mden[i][i2][1].number().isOne()) {
									mden[i][i2].setToChild(1, true, &mden[i], i2 + 1);
								} else {
									mden[i][i2].calculateRaiseExponent(eo);
								}
								break;
							}
						}
						break;
					}
					case STRUCT_POWER: {
						if(mden[i][1].isNumber() && mden[i][1].number().isReal() && mnum.equals(mden[i][0])) {
							mden[i][1].number() -= nr;
							if(mden[i][1].number().isOne()) {
								mden[i].setToChild(1, true, &mden, i + 1);
							} else {
								mden[i].calculateRaiseExponent(eo, &mden, i);
							}
							break;
						}
					}
					default: {
						if(!nr.isOne()) {
							MathStructure *mexp = new MathStructure(1, 1, 0);
							mexp->number() -= nr;
							mden[i].raise_nocopy(mexp);
							mden[i].calculateRaiseExponent(eo, &mden, 1);
						} else {
							mden[i].set(m_one);
						}
					}
				}
			}
			mden.calculatesub(eo, eo, false);
		}
	}
}

bool addablePower(const MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct[0].representsNonNegative(true)) return true;
	if(mstruct[1].representsInteger()) return true;
	//return eo.allow_complex && mstruct[0].representsNegative(true) && mstruct[1].isNumber() && mstruct[1].number().isRational() && mstruct[1].number().denominatorIsEven();
	return eo.allow_complex && mstruct[1].isNumber() && mstruct[1].number().isRational() && mstruct[1].number().denominatorIsEven();
}

int MathStructure::merge_multiplication(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this, size_t index_mstruct, bool reversed, bool do_append) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.multiply(mstruct.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	if(mstruct.isOne()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	} else if(isOne()) {
		if(mparent) {
			mparent->swapChildren(index_this + 1, index_mstruct + 1);
		} else {
			set_nocopy(mstruct, true);
		}
		return 3;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite()) {
		if(o_number.isMinusInfinity()) {
			if(mstruct.representsPositive(false)) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 2;
			} else if(mstruct.representsNegative(false)) {
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(o_number.isPlusInfinity()) {
			if(mstruct.representsPositive(false)) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 2;
			} else if(mstruct.representsNegative(false)) {
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite()) {
		if(mstruct.number().isMinusInfinity()) {
			if(representsPositive(false)) {
				clear(true);
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(representsNegative(false)) {
				clear(true);
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(mstruct.number().isPlusInfinity()) {
			if(representsPositive(false)) {
				clear(true);
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(representsNegative(false)) {
				clear(true);
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		}
	}

	if(representsUndefined() || mstruct.representsUndefined()) return -1;

	// x/(x^2+x)=1/(x+1)
	const MathStructure *mnum = NULL, *mden = NULL;
	bool b_nonzero = false;
	if(eo.reduce_divisions) {
		if(!isNumber() && mstruct.isPower() && mstruct[0].isAddition() && mstruct[0].size() > 1 && mstruct[1].isNumber() && mstruct[1].number().isMinusOne()) {
			if((!isPower() || !CHILD(1).hasNegativeSign()) && representsNumber() && mstruct[0].representsNumber()) {
				if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mstruct[0].representsZero(true)) || mstruct[0].representsNonZero(true)) {
					b_nonzero = true;
				}
				if(b_nonzero || (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mstruct[0].representsZero(true))) {
					mnum = this;
					mden = &mstruct[0];
				}
			}
		} else if(!mstruct.isNumber() && isPower() && CHILD(0).isAddition() && CHILD(0).size() > 1 && CHILD(1).isNumber() && CHILD(1).number().isMinusOne()) {
			if((!mstruct.isPower() || !mstruct[1].hasNegativeSign()) && mstruct.representsNumber() && CHILD(0).representsNumber()) {
				if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true)) || CHILD(0).representsNonZero(true)) {
					b_nonzero = true;
				}
				if(b_nonzero || (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true))) {
					mnum = &mstruct;
					mden = &CHILD(0);
				}
			}
		}
	}

	if(mnum && mden && eo.reduce_divisions) {
		switch(mnum->type()) {
			case STRUCT_ADDITION: {
				break;
			}
			case STRUCT_MULTIPLICATION: {
				Number nr;
				vector<Number> nrs;
				vector<size_t> reducables;
				for(size_t i = 0; i < mnum->size(); i++) {
					switch((*mnum)[i].type()) {
						case STRUCT_ADDITION: {break;}
						case STRUCT_POWER: {
							if((*mnum)[i][1].isNumber() && (*mnum)[i][1].number().isReal()) {
								if((*mnum)[i][1].number().isPositive()) {
									nr.set((*mnum)[i][1].number());
									if(reducable((*mnum)[i][0], *mden, nr)) {
										nrs.push_back(nr);
										reducables.push_back(i);
									}
								}
								break;
							}
						}
						default: {
							nr.set(1, 1, 0);
							if(reducable((*mnum)[i], *mden, nr)) {
								nrs.push_back(nr);
								reducables.push_back(i);
							}
						}
					}
				}
				if(reducables.size() > 0) {
					if(!b_nonzero && eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(*mden, eo)) break;
					if(mnum == this) {
						mstruct.ref();
						transform_nocopy(STRUCT_MULTIPLICATION, &mstruct);
					} else {
						transform(STRUCT_MULTIPLICATION);
						PREPEND_REF(&mstruct);
					}
					size_t i_erased = 0;
					for(size_t i = 0; i < reducables.size(); i++) {
						switch(CHILD(0)[reducables[i] - i_erased].type()) {
							case STRUCT_POWER: {
								if(CHILD(0)[reducables[i] - i_erased][1].isNumber() && CHILD(0)[reducables[i] - i_erased][1].number().isReal()) {
									reduce(CHILD(0)[reducables[i] - i_erased][0], CHILD(1)[0], nrs[i], eo);
									if(nrs[i] == CHILD(0)[reducables[i] - i_erased][1].number()) {
										CHILD(0).delChild(reducables[i] - i_erased + 1);
										i_erased++;
									} else {
										CHILD(0)[reducables[i] - i_erased][1].number() -= nrs[i];
										if(CHILD(0)[reducables[i] - i_erased][1].number().isOne()) {
											CHILD(0)[reducables[i] - i_erased].setToChild(1, true, &CHILD(0), reducables[i] - i_erased + 1);
										} else {
											CHILD(0)[reducables[i] - i_erased].calculateRaiseExponent(eo);
										}
										CHILD(0).calculateMultiplyIndex(reducables[i] - i_erased, eo, true);
									}
									break;
								}
							}
							default: {
								reduce(CHILD(0)[reducables[i] - i_erased], CHILD(1)[0], nrs[i], eo);
								if(nrs[i].isOne()) {
									CHILD(0).delChild(reducables[i] - i_erased + 1);
									i_erased++;
								} else {
									MathStructure mexp(1, 1);
									mexp.number() -= nrs[i];
									CHILD(0)[reducables[i] - i_erased].calculateRaise(mexp, eo);
									CHILD(0).calculateMultiplyIndex(reducables[i] - i_erased, eo, true);
								}
							}
						}
					}
					if(CHILD(0).size() == 0) {
						setToChild(2, true, mparent, index_this + 1);
					} else if(CHILD(0).size() == 1) {
						CHILD(0).setToChild(1, true, this, 1);
						calculateMultiplyIndex(0, eo, true, mparent, index_this);
					} else {
						calculateMultiplyIndex(0, eo, true, mparent, index_this);
					}
					return 1;
				}
				break;
			}
			case STRUCT_POWER: {
				if((*mnum)[1].isNumber() && (*mnum)[1].number().isReal()) {
					if((*mnum)[1].number().isPositive()) {
						Number nr((*mnum)[1].number());
						if(reducable((*mnum)[0], *mden, nr)) {
							if(!b_nonzero && eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(*mden, eo)) break;
							if(nr != (*mnum)[1].number()) {
								MathStructure mnum2((*mnum)[0]);
								if(mnum == this) {
									CHILD(1).number() -= nr;
									if(CHILD(1).number().isOne()) {
										set(mnum2);
									} else {
										calculateRaiseExponent(eo);
									}
									mstruct.ref();
									transform_nocopy(STRUCT_MULTIPLICATION, &mstruct);
									reduce(mnum2, CHILD(1)[0], nr, eo);
									calculateMultiplyLast(eo);
								} else {
									transform(STRUCT_MULTIPLICATION);
									PREPEND(mstruct);
									CHILD(0)[1].number() -= nr;
									if(CHILD(0)[1].number().isOne()) {
										CHILD(0) = mnum2;
									} else {
										CHILD(0).calculateRaiseExponent(eo);
									}
									reduce(mnum2, CHILD(1)[0], nr, eo);
									calculateMultiplyIndex(0, eo);
								}
							} else {
								if(mnum == this) {
									MathStructure mnum2((*mnum)[0]);
									if(mparent) {
										mparent->swapChildren(index_this + 1, index_mstruct + 1);
										reduce(mnum2, (*mparent)[index_this][0], nr, eo);
									} else {
										set_nocopy(mstruct, true);
										reduce(mnum2, CHILD(0), nr, eo);
									}
								} else {
									reduce((*mnum)[0], CHILD(0), nr, eo);
								}
							}
							return 1;
						}
					}
					break;
				}
			}
			default: {
				Number nr(1, 1);
				if(reducable(*mnum, *mden, nr)) {
					if(!b_nonzero && eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(*mden, eo)) break;
					if(mnum == this) {
						MathStructure mnum2(*mnum);
						if(mparent) {
							mparent->swapChildren(index_this + 1, index_mstruct + 1);
							reduce(mnum2, (*mparent)[index_this][0], nr, eo);
							(*mparent)[index_this].calculateRaiseExponent(eo, mparent, index_this);
						} else {
							set_nocopy(mstruct, true);
							reduce(mnum2, CHILD(0), nr, eo);
							calculateRaiseExponent(eo, mparent, index_this);
						}						
					} else {
						reduce(*mnum, CHILD(0), nr, eo);
						calculateRaiseExponent(eo, mparent, index_this);
					}
					return 1;
				}
			}
		}
	}

	if(mstruct.isFunction() && eo.protected_function != mstruct.function()) {
		if(((mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_signum && mstruct[0].size() == 2) || (mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2 && mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_abs && mstruct[0].size() == 1)) && (equals(mstruct[0][0]) || (isFunction() && o_function == CALCULATOR->f_abs && SIZE == 1 && CHILD(0) == mstruct[0][0]) || (isPower() && CHILD(0) == mstruct[0][0]) || (isPower() && CHILD(0).isFunction() && CHILD(0).function() == CALCULATOR->f_abs && CHILD(0).size() == 1 && CHILD(0)[0] == mstruct[0][0]))) {
				// sgn(abs(x))*x^y=x^y
				MERGE_APPROX_AND_PREC(mstruct)
				return 2;
		} else if(mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2) {
			if(equals(mstruct[0]) && representsReal(true)) {
				// sgn(x)*x=abs(x)
				transform(CALCULATOR->f_abs);
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(isPower() && CHILD(1).representsOdd() && mstruct[0] == CHILD(0) && CHILD(0).representsReal(true)) {
				//sgn(x)*x^3=abs(x)^3
				CHILD(0).transform(CALCULATOR->f_abs);
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct)) {
			if(equals(mstruct[0]) && mstruct[0].representsReal(true) && mstruct[1].number().isOdd()) {
				// root(x, 3)*x=abs(x)^(1/3)*x
				mstruct[0].transform(STRUCT_FUNCTION);
				mstruct[0].setFunction(CALCULATOR->f_abs);
				mstruct[1].number().recip();
				mstruct.setType(STRUCT_POWER);
				transform(STRUCT_FUNCTION);
				setFunction(CALCULATOR->f_abs);
				mstruct.ref();
				multiply_nocopy(&mstruct);
				calculateMultiplyLast(eo);
				return 1;
			} else if(isPower() && CHILD(1).representsOdd() && CHILD(0) == mstruct[0] && CHILD(0).representsReal(true) && mstruct[1].number().isOdd()) {
				// root(x, 3)*x^3=abs(x)^(1/3)*x^3
				mstruct[0].transform(STRUCT_FUNCTION);
				mstruct[0].setFunction(CALCULATOR->f_abs);
				mstruct[1].number().recip();
				mstruct.setType(STRUCT_POWER);
				CHILD(0).transform(STRUCT_FUNCTION);
				CHILD(0).setFunction(CALCULATOR->f_abs);
				mstruct.ref();
				multiply_nocopy(&mstruct);
				calculateMultiplyLast(eo);
				return 1;
			}
		}
	}
	if(isZero()) {
		if(mstruct.isFunction()) {
			if((mstruct.function() == CALCULATOR->f_ln || mstruct.function() == CALCULATOR->f_Ei) && mstruct.size() == 1) {
				if(mstruct[0].representsNonZero() || warn_about_assumed_not_value(mstruct[0], m_zero, eo)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 2;
				}
			} else if(mstruct.function() == CALCULATOR->f_li && mstruct.size() == 1) {
				if(mstruct.representsNumber(true) || warn_about_assumed_not_value(mstruct[0], m_one, eo)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 2;
				}
			}
		} else if(mstruct.isPower() && mstruct[0].isFunction() && mstruct[1].representsNumber()) {
			if((mstruct[0].function() == CALCULATOR->f_ln || mstruct[0].function() == CALCULATOR->f_Ei) && mstruct[0].size() == 1) {
				if(mstruct[0][0].representsNonZero() || warn_about_assumed_not_value(mstruct[0][0], m_zero, eo)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 2;
				}
			} else if(mstruct[0].function() == CALCULATOR->f_li && mstruct[0].size() == 1) {
				if(mstruct[0].representsNumber(true) || warn_about_assumed_not_value(mstruct[0][0], m_one, eo)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 2;
				}
			}
		}
	}

	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_ADDITION: {
					return 0;
				}
				case STRUCT_VECTOR: {
					if(isMatrix() && mstruct.isMatrix()) {
						if(CHILD(0).size() != mstruct.size()) {
							CALCULATOR->error(true, _("The second matrix must have as many rows (was %s) as the first has columns (was %s) for matrix multiplication."), i2s(mstruct.size()).c_str(), i2s(CHILD(0).size()).c_str(), NULL);
							return -1;
						}
						MathStructure msave(*this);
						size_t rows = SIZE;
						clearMatrix(true);
						resizeMatrix(rows, mstruct[0].size(), m_zero);
						MathStructure mtmp;
						for(size_t index_r = 0; index_r < SIZE; index_r++) {
							for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
								for(size_t index = 0; index < msave[0].size(); index++) {
									mtmp = msave[index_r][index];
									mtmp.calculateMultiply(mstruct[index][index_c], eo);
									CHILD(index_r)[index_c].calculateAdd(mtmp, eo, &CHILD(index_r), index_c);
								}			
							}		
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else if(isMatrix() && mstruct.isVector()) {
						if(SIZE != mstruct.size() || CHILD(0).size() != 1) {
							CALCULATOR->error(true, _("The second matrix must have as many rows (was %s) as the first has columns (was %s) for matrix multiplication."), i2s(1).c_str(), i2s(CHILD(0).size()).c_str(), NULL);
							return -1;
						}
						MathStructure msave(*this);
						size_t rows = SIZE;
						clearMatrix(true);
						resizeMatrix(rows, mstruct.size(), m_zero);
						MathStructure mtmp;
						for(size_t index_r = 0; index_r < SIZE; index_r++) {
							for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
								CHILD(index_r)[index_c].set(msave[index_r][0]);
								CHILD(index_r)[index_c].calculateMultiply(mstruct[index_c], eo);
							}
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else {
						if(SIZE == mstruct.size()) {
							for(size_t i = 0; i < SIZE; i++) {
								mstruct[i].ref();
								CHILD(i).multiply_nocopy(&mstruct[i], true);
								CHILD(i).calculateMultiplyLast(eo, true);
							}
							m_type = STRUCT_ADDITION;
							MERGE_APPROX_AND_PREC(mstruct)
							calculatesub(eo, eo, false, mparent, index_this);
							return 1;
						}
					}
					return -1;
				}
				default: {
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).calculateMultiply(mstruct, eo);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					calculatesub(eo, eo, false, mparent, index_this);
					return 1;
				}
			}
		}
		case STRUCT_ADDITION: {
			switch(mstruct.type()) {
				case STRUCT_ADDITION: {
					if(eo.expand != 0 && (eo.expand > -2 || (!containsInterval(true, false, false, eo.expand == -2) && !mstruct.containsInterval(true, false, false, eo.expand == -2)) || (representsNonNegative(true) && mstruct.representsNonNegative(true)))) {
						MathStructure msave(*this);
						CLEAR;
						for(size_t i = 0; i < mstruct.size(); i++) {
							if(CALCULATOR->aborted()) {
								set(msave);
								return -1;
							}
							APPEND(msave);
							mstruct[i].ref();
							LAST.multiply_nocopy(&mstruct[i], true);
							if(reversed) {
								LAST.swapChildren(1, LAST.size());
								LAST.calculateMultiplyIndex(0, eo, true, this, SIZE - 1);
							} else {
								LAST.calculateMultiplyLast(eo, true, this, SIZE - 1);
							}
						}
						MERGE_APPROX_AND_PREC(mstruct)
						calculatesub(eo, eo, false, mparent, index_this);
						return 1;
					} else if(eo.expand <= -2 && (!mstruct.containsInterval(true, false, false, eo.expand == -2) || representsNonNegative(true))) {
						for(size_t i = 0; i < SIZE; i++) {
							CHILD(i).calculateMultiply(mstruct, eo, this, i);
						}
						calculatesub(eo, eo, false, mparent, index_this);
						return 1;
					} else if(eo.expand <= -2 && (!containsInterval(true, false, false, eo.expand == -2) || mstruct.representsNonNegative(true))) {
						return 0;
					} else {
						if(equals(mstruct)) {
							raise_nocopy(new MathStructure(2, 1, 0));
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					break;
				}
				case STRUCT_POWER: {
					if(mstruct[1].isNumber() && *this == mstruct[0]) {
						if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !representsZero(true)) 
						|| (mstruct[1].isNumber() && mstruct[1].number().isReal() && !mstruct[1].number().isMinusOne())
						|| representsNonZero(true) 
						|| mstruct[1].representsPositive()
						|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !representsZero(true) && warn_about_denominators_assumed_nonzero_or_positive(*this, mstruct[1], eo))) {
							if(mparent) {
								mparent->swapChildren(index_this + 1, index_mstruct + 1);
								(*mparent)[index_this][1].number()++;
								(*mparent)[index_this].calculateRaiseExponent(eo, mparent, index_this);
							} else {
								set_nocopy(mstruct, true);
								CHILD(1).number()++;
								calculateRaiseExponent(eo, mparent, index_this);
							}
							return 1;
						}
					}
					if(eo.expand == 0 && mstruct[0].isAddition()) return -1;
					if(eo.combine_divisions && mstruct[1].hasNegativeSign()) {
						int ret;
						vector<bool> merged;
						merged.resize(SIZE, false);
						size_t merges = 0;
						MathStructure *mstruct2 = new MathStructure(mstruct);
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isOne()) ret = -1;
							else ret = CHILD(i).merge_multiplication(*mstruct2, eo, NULL, 0, 0, false, false);
							if(ret == 0) {
								ret = mstruct2->merge_multiplication(CHILD(i), eo, NULL, 0, 0, true, false);
								if(ret >= 1) {
									mstruct2->ref();
									setChild_nocopy(mstruct2, i + 1);
								}
							}
							if(ret >= 1) {
								mstruct2->unref();
								if(i + 1 != SIZE) mstruct2 = new MathStructure(mstruct);
								merged[i] = true;
								merges++;
							} else {
								if(i + 1 == SIZE) mstruct2->unref();
								merged[i] = false;
							}
						}
						if(merges == 0) {
							return -1;
						} else if(merges == SIZE) {
							calculatesub(eo, eo, false, mparent, index_this);
							return 1;
						} else if(merges == SIZE - 1) {
							for(size_t i = 0; i < SIZE; i++) {
								if(!merged[i]) {
									mstruct.ref();
									CHILD(i).multiply_nocopy(&mstruct, true);
									break;
								}
							}
							calculatesub(eo, eo, false, mparent, index_this);
						} else {
							MathStructure *mdiv = new MathStructure();
							merges = 0;
							for(size_t i = 0; i - merges < SIZE; i++) {
								if(!merged[i]) {
									CHILD(i - merges).ref();
									if(merges > 0) {
										(*mdiv)[0].add_nocopy(&CHILD(i - merges), merges > 1);
									} else {
										mdiv->multiply(mstruct);
										mdiv->setChild_nocopy(&CHILD(i - merges), 1);
									}
									ERASE(i - merges);
									merges++;
								}
							}
							add_nocopy(mdiv, true);
							calculatesub(eo, eo, false);
						}
						return 1;
					}
					if(eo.expand == 0 || (eo.expand < -1 && mstruct.containsInterval(true, false, false, eo.expand == -2) && !representsNonNegative(true))) return -1;
				}
				case STRUCT_MULTIPLICATION: {
					if(do_append && (eo.expand == 0 || (eo.expand < -1 && mstruct.containsInterval(true, false, false, eo.expand == -2) && !representsNonNegative(true)))) {
						transform(STRUCT_MULTIPLICATION);
						for(size_t i = 0; i < mstruct.size(); i++) {
							APPEND_REF(&mstruct[i]);
						}
						return 1;
					}
				}
				default: {
					if(eo.expand == 0 || (eo.expand < -1 && mstruct.containsInterval(true, false, false, eo.expand == -2) && !representsNonNegative(true))) return -1;
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).multiply(mstruct, true);
						if(reversed) {
							CHILD(i).swapChildren(1, CHILD(i).size());
							CHILD(i).calculateMultiplyIndex(0, eo, true, this, i);
						} else {
							CHILD(i).calculateMultiplyLast(eo, true, this, i);
						}
					}
					MERGE_APPROX_AND_PREC(mstruct)
					calculatesub(eo, eo, false, mparent, index_this);
					return 1;
				}
			}
			return -1;
		}
		case STRUCT_MULTIPLICATION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {
					if(eo.expand == 0) {
						if(!do_append) return -1;
						APPEND_REF(&mstruct);
						return 1;
					}
					return 0;
				}
				case STRUCT_MULTIPLICATION: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						if(reversed) {
							PREPEND_REF(&mstruct[i]);
							calculateMultiplyIndex(0, eo, false);
						} else {
							APPEND_REF(&mstruct[i]);
							calculateMultiplyLast(eo, false);
						}
					}
					MERGE_APPROX_AND_PREC(mstruct)
					if(SIZE == 1) {
						setToChild(1, false, mparent, index_this + 1);
					} else if(SIZE == 0) {
						clear(true);
					} else {
						evalSort();
					}					
					return 1;
				}
				case STRUCT_POWER: {
					if(mstruct[1].isNumber()) {
						if(*this == mstruct[0]) {
							if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !representsZero(true)) 
							|| (mstruct[1].isNumber() && mstruct[1].number().isReal() && !mstruct[1].number().isMinusOne()) 
							|| representsNonZero(true) 
							|| mstruct[1].representsPositive()
							|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !representsZero(true) && warn_about_denominators_assumed_nonzero_or_positive(*this, mstruct[1], eo))) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
									(*mparent)[index_this][1].number()++;
									(*mparent)[index_this].calculateRaiseExponent(eo, mparent, index_this);
								} else {
									set_nocopy(mstruct, true);
									CHILD(1).number()++;
									calculateRaiseExponent(eo, mparent, index_this);
								}							
								return 1;
							}
						} else {
							for(size_t i = 0; i < SIZE; i++) {
								int ret = CHILD(i).merge_multiplication(mstruct, eo, NULL, 0, 0, false, false);
								if(ret == 0) {
									ret = mstruct.merge_multiplication(CHILD(i), eo, NULL, 0, 0, true, false);
									if(ret >= 1) {
										if(ret == 2) ret = 3;
										else if(ret == 3) ret = 2;
										mstruct.ref();
										setChild_nocopy(&mstruct, i + 1);
									}
								}
								if(ret >= 1) {
									if(ret != 2) calculateMultiplyIndex(i, eo, true, mparent, index_this);
									return 1;
								}	
							}
						}
					}
				}
				default: {
					if(do_append) {
						MERGE_APPROX_AND_PREC(mstruct)
						if(reversed) {
							PREPEND_REF(&mstruct);
							calculateMultiplyIndex(0, eo, true, mparent, index_this);
						} else {
							APPEND_REF(&mstruct);
							calculateMultiplyLast(eo, true, mparent, index_this);
						}
						return 1;
					}
				}
			}
			return -1;
		}
		case STRUCT_POWER: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {
					return 0;
				}
				case STRUCT_POWER: {
					if(mstruct[0] == CHILD(0)) {
						if(mstruct[0].isUnit() && mstruct[0].prefix()) CHILD(0).setPrefix(mstruct[0].prefix());
						bool b = eo.allow_complex || CHILD(0).representsNonNegative(true), b2 = true, b_warn = false;
						if(!b) {
							b = CHILD(1).representsInteger() && mstruct[1].representsInteger();
						}
						if(b) {
							b = false;
							bool b2test = false;
							if(IS_REAL(mstruct[1]) && IS_REAL(CHILD(1))) {
								if(mstruct[1].number().isPositive() == CHILD(1).number().isPositive()) {
									b2 = true;
									b = true;
								} else if(!mstruct[1].number().isMinusOne() && !CHILD(1).number().isMinusOne()) {
									b2 = (mstruct[1].number() + CHILD(1).number()).isNegative();
									b = true;
									if(!b2) b2test = true;
								}
							}
							if(!b || b2test) {
								b = (!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true)) 
								|| CHILD(0).representsNonZero(true) 
								|| (CHILD(1).representsPositive() && mstruct[1].representsPositive()) 
								|| (CHILD(1).representsNegative() && mstruct[1].representsNegative());
								if(!b && eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true)) {
									b = true;
									b_warn = true;
								}
								if(b2test) {
									b2 = b;
									b = true;
								}
							}
						}
						if(b) {
							if(IS_REAL(CHILD(1)) && IS_REAL(mstruct[1])) {
								if(!b2 && !do_append) return -1;
								if(b_warn && !warn_about_denominators_assumed_nonzero(CHILD(0), eo)) return -1;
								if(b2) {
									CHILD(1).number() += mstruct[1].number();
									calculateRaiseExponent(eo, mparent, index_this);
								} else {
									if(CHILD(1).number().isNegative()) {
										CHILD(1).number()++;
										mstruct[1].number() += CHILD(1).number();
										CHILD(1).number().set(-1, 1, 0);
									} else {
										mstruct[1].number()++;
										CHILD(1).number() += mstruct[1].number();
										mstruct[1].number().set(-1, 1, 0);
									}
									MERGE_APPROX_AND_PREC(mstruct)
									transform(STRUCT_MULTIPLICATION);
									CHILD(0).calculateRaiseExponent(eo, this, 0);
									if(reversed) {
										PREPEND_REF(&mstruct);
										CHILD(0).calculateRaiseExponent(eo, this, 0);
										calculateMultiplyIndex(0, eo, true, mparent, index_this);
									} else {
										APPEND_REF(&mstruct);
										CHILD(1).calculateRaiseExponent(eo, this, 1);
										calculateMultiplyLast(eo, true, mparent, index_this);
									}
								}
								return 1;
							} else {
								MathStructure mstruct2(CHILD(1));
								if(mstruct2.calculateAdd(mstruct[1], eo)) {
									if(b_warn && !warn_about_denominators_assumed_nonzero_llgg(CHILD(0), CHILD(1), mstruct[1], eo)) return -1;
									CHILD(1) = mstruct2;
									calculateRaiseExponent(eo, mparent, index_this);
									return 1;
								}
							}
						}
					} else if(mstruct[1] == CHILD(1)) {
						if(!CHILD(0).isMultiplication() && !mstruct[0].isMultiplication() && (mstruct[1].representsInteger() || CHILD(0).representsPositive(true) || mstruct[0].representsPositive(true))) {
							MathStructure mstruct2(CHILD(0));
							if(mstruct2.calculateMultiply(mstruct[0], eo)) {
								CHILD(0) = mstruct2;
								MERGE_APPROX_AND_PREC(mstruct)
								calculateRaiseExponent(eo, mparent, index_this);
								return 1;
							}
						} else if(CHILD(1).representsInteger() && CHILD(0).isFunction() && mstruct[0].isFunction() && ((CHILD(0).function() == CALCULATOR->f_cos && mstruct[0].function() == CALCULATOR->f_sin) || (CHILD(0).function() == CALCULATOR->f_sin && mstruct[0].function() == CALCULATOR->f_cos) || (CHILD(0).function() == CALCULATOR->f_cosh && mstruct[0].function() == CALCULATOR->f_sinh) || (CHILD(0).function() == CALCULATOR->f_sinh && mstruct[0].function() == CALCULATOR->f_cosh)) && CHILD(0).size() == 1 && mstruct[0].size() == 1 && CHILD(0)[0] == mstruct[0][0]) {
							// cos(x)^n*sin(x)^n=sin(2x)^n/2^n
							if(CHILD(0).function() == CALCULATOR->f_cosh) CHILD(0).setFunction(CALCULATOR->f_sinh);
							else if(CHILD(0).function() == CALCULATOR->f_cos) CHILD(0).setFunction(CALCULATOR->f_sin);
							CHILD(0)[0].calculateMultiply(nr_two, eo);
							CHILD(0).childUpdated(1);
							CHILD_UPDATED(0)
							MathStructure *mdiv = new MathStructure(2, 1, 0);
							mdiv->calculateRaise(CHILD(1), eo);
							mdiv->calculateInverse(eo);
							multiply_nocopy(mdiv);
							calculateMultiplyLast(eo);
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					break;
				}
				case STRUCT_FUNCTION: {
					if(eo.protected_function != CALCULATOR->f_signum && mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2 && CHILD(0).isFunction() && CHILD(0).function() == CALCULATOR->f_abs && CHILD(0).size() == 1 && mstruct[0] == CHILD(0)[0] && CHILD(1).isNumber() && CHILD(1).number().isRational() && CHILD(1).number().numeratorIsOne() && !CHILD(1).number().denominatorIsEven() && CHILD(0)[0].representsReal(true)) {
						setType(STRUCT_FUNCTION);
						setFunction(CALCULATOR->f_root);
						CHILD(0).setToChild(1, true);
						CHILD(1).number().recip();
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
				default: {
					if(!mstruct.isNumber() && CHILD(1).isNumber() && CHILD(0) == mstruct) {
						if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true)) 
						|| (CHILD(1).isNumber() && CHILD(1).number().isReal() && !CHILD(1).number().isMinusOne())
						|| CHILD(0).representsNonZero(true) 
						|| CHILD(1).representsPositive()
						|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !CHILD(0).representsZero(true) && warn_about_denominators_assumed_nonzero_or_positive(CHILD(0), CHILD(1), eo))) {
							CHILD(1).number()++;
							MERGE_APPROX_AND_PREC(mstruct)
							calculateRaiseExponent(eo, mparent, index_this);
							return 1;
						}
					}
					if(mstruct.isZero() && (!eo.keep_zero_units || containsType(STRUCT_UNIT, true, true) == 0 || (CHILD(0).isUnit() && CHILD(0).unit() == CALCULATOR->u_rad)) && !representsUndefined(true, true, !eo.assume_denominators_nonzero) && representsNonMatrix()) {
						clear(true);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					if(CHILD(0).isFunction() && mstruct.isZero() && CHILD(1).representsNumber()) {
						if((CHILD(0).function() == CALCULATOR->f_ln || CHILD(0).function() == CALCULATOR->f_Ei) && SIZE == 1) {
							if(CHILD(0)[0].representsNonZero() || warn_about_assumed_not_value(CHILD(0)[0], m_zero, eo)) {
								clear(true);
								MERGE_APPROX_AND_PREC(mstruct)
								return 3;
							}
						} else if(CHILD(0).function() == CALCULATOR->f_li && SIZE == 1) {
							if(CHILD(0).representsNumber(true) || warn_about_assumed_not_value(CHILD(0)[0], m_one, eo)) {
								clear(true);
								MERGE_APPROX_AND_PREC(mstruct)
								return 3;
							}
						}
					}
					break;
				}
			}
			return -1;
		}
		case STRUCT_FUNCTION: {
			if(eo.protected_function != o_function) {
				if(((o_function == CALCULATOR->f_abs && SIZE == 1 && CHILD(0).isFunction() && CHILD(0).function() == CALCULATOR->f_signum && CHILD(0).size() == 2) || (o_function == CALCULATOR->f_signum && SIZE == 2 && CHILD(0).isFunction() && CHILD(0).function() == CALCULATOR->f_abs && CHILD(0).size() == 1)) && (CHILD(0)[0] == mstruct || (mstruct.isFunction() && mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && CHILD(0)[0] == mstruct[0]) || (mstruct.isPower() && mstruct[0] == CHILD(0)[0]) || (mstruct.isPower() && mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_abs && mstruct[0].size() == 1 && CHILD(0)[0] == mstruct[0][0]))) {
					// sgn(abs(x))*x^y=x^y
					if(mparent) {
						mparent->swapChildren(index_this + 1, index_mstruct + 1);
					} else {
						set_nocopy(mstruct, true);
					}
					return 3;
				} else if(o_function == CALCULATOR->f_abs && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && mstruct[0] == CHILD(0) && CHILD(0).representsReal(true)) {
						// abs(x)*abs(x)=x^2
						SET_CHILD_MAP(0)
						MERGE_APPROX_AND_PREC(mstruct)
						calculateRaise(nr_two, eo);
						return 1;
					} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2 && mstruct[0] == CHILD(0) && CHILD(0).representsScalar()) {
						// sgn(x)*abs(x)=x
						SET_CHILD_MAP(0)
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if(o_function == CALCULATOR->f_signum && SIZE == 2) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2 && mstruct[0] == CHILD(0) && CHILD(0).representsReal(true)) {
						if(mstruct[1].isOne() && CHILD(1).isOne()) {
							set(1, 1, 0, true);
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						} else if(mstruct[1] == CHILD(1)) {
							// sgn(x)*sgn(x)=sgn(abs(x))
							CHILD(0).transform(CALCULATOR->f_abs);
							CHILD_UPDATED(0)
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && mstruct[0] == CHILD(0) && CHILD(0).representsScalar()) {
						// sgn(x)*abs(x)=x
						SET_CHILD_MAP(0)
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else if(mstruct == CHILD(0) && CHILD(0).representsReal(true)) {
						// sgn(x)*x=abs(x)
						setFunction(CALCULATOR->f_abs);
						ERASE(1)
						return 1;
					} else if(mstruct.isPower() && mstruct[1].representsOdd() && mstruct[0] == CHILD(0) && CHILD(0).representsReal(true)) {
						//sgn(x)*x^3=abs(x)^3
						mstruct[0].transform(STRUCT_FUNCTION);
						mstruct[0].setFunction(CALCULATOR->f_abs);
						if(mparent) {
							mparent->swapChildren(index_this + 1, index_mstruct + 1);
						} else {
							set_nocopy(mstruct, true);
						}
						return 1;
					}
				} else if(o_function == CALCULATOR->f_root && THIS_VALID_ROOT) {
					if(CHILD(0) == mstruct && CHILD(0).representsReal(true) && CHILD(1).number().isOdd()) {
						// root(x, 3)*x=abs(x)^(1/3)*x
						CHILD(0).transform(STRUCT_FUNCTION);
						CHILD(0).setFunction(CALCULATOR->f_abs);
						CHILD(1).number().recip();
						m_type = STRUCT_POWER;
						mstruct.transform(STRUCT_FUNCTION);
						mstruct.setFunction(CALCULATOR->f_abs);
						mstruct.ref();
						multiply_nocopy(&mstruct);
						calculateMultiplyLast(eo);
						return 1;
					} else if(mstruct.isPower() && mstruct[1].representsOdd() && CHILD(0) == mstruct[0] && CHILD(0).representsReal(true) && CHILD(1).number().isOdd()) {
						// root(x, 3)*x^3=abs(x)^(1/3)*x^3
						CHILD(0).transform(STRUCT_FUNCTION);
						CHILD(0).setFunction(CALCULATOR->f_abs);
						CHILD(1).number().recip();
						m_type = STRUCT_POWER;
						mstruct[0].transform(STRUCT_FUNCTION);
						mstruct[0].setFunction(CALCULATOR->f_abs);
						mstruct.ref();
						multiply_nocopy(&mstruct);
						calculateMultiplyLast(eo);
						return 1;
					} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct) && CHILD(0) == mstruct[0] && CHILD(0).representsReal(true) && CHILD(1).number().isOdd() && mstruct[1].number().isOdd()) {
						// root(x, y)*root(x, z)=abs(x)^(1/y)*abs(x)^(1/z)
						CHILD(0).transform(STRUCT_FUNCTION);
						CHILD(0).setFunction(CALCULATOR->f_abs);
						CHILD(1).number().recip();
						m_type = STRUCT_POWER;
						mstruct[0].transform(STRUCT_FUNCTION);
						mstruct[0].setFunction(CALCULATOR->f_abs);
						mstruct[1].number().recip();
						mstruct.setType(STRUCT_POWER);
						mstruct.ref();
						multiply_nocopy(&mstruct);
						calculateMultiplyLast(eo);
						return 1;
					}
				} else if(o_function == CALCULATOR->f_tan && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_cos && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// tan(x)*cos(x)=sin(x)
						setFunction(CALCULATOR->f_sin);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if(o_function == CALCULATOR->f_cos && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_tan && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// tan(x)*cos(x)=sin(x)
						setFunction(CALCULATOR->f_sin);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_sin && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// cos(x)*sin(x)=sin(2x)/2
						setFunction(CALCULATOR->f_sin);
						CHILD(0).calculateMultiply(nr_two, eo);
						CHILD_UPDATED(0)
						divide(nr_two);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if(o_function == CALCULATOR->f_sin && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_cos && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// cos(x)*sin(x)=sin(2x)/2
						setFunction(CALCULATOR->f_sin);
						CHILD(0).calculateMultiply(nr_two, eo);
						CHILD_UPDATED(0)
						divide(nr_two);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if(o_function == CALCULATOR->f_sinh && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_cosh && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// cosh(x)*sinh(x)=sinh(2x)/2
						setFunction(CALCULATOR->f_sinh);
						CHILD(0).calculateMultiply(nr_two, eo);
						CHILD_UPDATED(0)
						divide(nr_two);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if(o_function == CALCULATOR->f_cosh && SIZE == 1) {
					if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_sinh && mstruct.size() == 1 && mstruct[0] == CHILD(0)) {
						// cosh(x)*sinh(x)=sinh(2x)/2
						setFunction(CALCULATOR->f_sinh);
						CHILD(0).calculateMultiply(nr_two, eo);
						CHILD_UPDATED(0)
						divide(nr_two);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				} else if((o_function == CALCULATOR->f_ln || o_function == CALCULATOR->f_Ei) && SIZE == 1 && mstruct.isZero()) {
					if(CHILD(0).representsNonZero() || warn_about_assumed_not_value(CHILD(0), m_zero, eo)) {
						clear(true);
						MERGE_APPROX_AND_PREC(mstruct)
						return 3;
					}
				} else if(o_function == CALCULATOR->f_li && SIZE == 1 && mstruct.isZero()) {
					if(representsNumber(true) || warn_about_assumed_not_value(CHILD(0), m_one, eo)) {
						clear(true);
						MERGE_APPROX_AND_PREC(mstruct)
						return 3;
					}
				}
			}
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {}
				case STRUCT_POWER: {
					return 0;
				}
				case STRUCT_COMPARISON: {
					if(isComparison()) {
						mstruct.ref();
						transform_nocopy(STRUCT_LOGICAL_AND, &mstruct);
						return 1;
					}
				}
				default: {
					if(mstruct.isZero() && (!eo.keep_zero_units || containsType(STRUCT_UNIT, false, true) <= 0 || (isUnit() && unit() == CALCULATOR->u_rad)) && !representsUndefined(true, true, !eo.assume_denominators_nonzero) && representsNonMatrix()) {
						clear(true); 
						MERGE_APPROX_AND_PREC(mstruct)
						return 3;
					}
					if(isZero() && !mstruct.representsUndefined(true, true, !eo.assume_denominators_nonzero) && (!eo.keep_zero_units || mstruct.containsType(STRUCT_UNIT, false, true) <= 0 || (mstruct.isUnit() && mstruct.unit() == CALCULATOR->u_rad)) && mstruct.representsNonMatrix()) {
						MERGE_APPROX_AND_PREC(mstruct)
						return 2;
					}
					if(equals(mstruct)) {
						if(mstruct.isUnit() && mstruct.prefix()) o_prefix = mstruct.prefix();
						raise_nocopy(new MathStructure(2, 1, 0));
						MERGE_APPROX_AND_PREC(mstruct)
						calculateRaiseExponent(eo, mparent, index_this);
						return 1;
					}
					break;
				}
			}
			break;
		}
	}
	return -1;
}

bool test_if_numerator_not_too_large(Number &vb, Number &ve) {
	if(!vb.isRational()) return false;
	if(!mpz_fits_slong_p(mpq_numref(ve.internalRational()))) return false;
	long int exp = labs(mpz_get_si(mpq_numref(ve.internalRational())));
	if(vb.isRational()) {
		if((long long int) exp * mpz_sizeinbase(mpq_numref(vb.internalRational()), 10) <= 1000000LL && (long long int) exp * mpz_sizeinbase(mpq_denref(vb.internalRational()), 10) <= 1000000LL) return true;
	}
	return false;
}


int MathStructure::merge_power(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this, size_t index_mstruct, bool) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		// number^number
		Number nr(o_number);
		if(nr.raise(mstruct.number(), eo.approximation < APPROXIMATION_APPROXIMATE) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
			// Exponentiation succeeded without inappropriate change in approximation status
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		if(!o_number.isMinusOne() && mstruct.number().isRational() && !mstruct.isInteger()) {
			if(o_number.isNegative()) {
				MathStructure mtest(*this);
				if(mtest.number().negate() && mtest.calculateRaise(mstruct, eo)) {
					set(mtest);
					MathStructure *mmul = new MathStructure(-1, 1, 0);
					mmul->calculateRaise(mstruct, eo);
					multiply_nocopy(mmul);
					calculateMultiplyLast(eo);
					return 1;
				}
			} else {
				Number exp_num(mstruct.number().numerator());
				if(!exp_num.isOne() && !exp_num.isMinusOne() && o_number.isPositive() && test_if_numerator_not_too_large(o_number, exp_num)) {
					// Try raise by exponent numerator if not very large
					nr = o_number;
					if(nr.raise(exp_num, eo.approximation < APPROXIMATION_APPROXIMATE) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
						o_number = nr;
						numberUpdated();
						nr.set(mstruct.number().denominator());
						nr.recip();
						calculateRaise(nr, eo, mparent, index_this);
						return 1;
					}
				}
				if(o_number.isPositive()) {
					Number nr_root(mstruct.number().denominator());
					if(eo.split_squares && o_number.isInteger() && nr_root.isLessThanOrEqualTo(LARGEST_RAISED_PRIME_EXPONENT)) {
						int root = nr_root.intValue();
						nr.set(1, 1, 0);
						bool b = true, overflow;
						long int val;
						while(b) {
							if(CALCULATOR->aborted()) break;
							b = false;
							overflow = false;
							val = o_number.lintValue(&overflow);
							if(overflow) {
								mpz_srcptr cval = mpq_numref(o_number.internalRational());
								for(size_t i = 0; root == 2 ? (i < NR_OF_SQUARE_PRIMES) : (RAISED_PRIMES[root - 3][i] != 0); i++) {
									if(CALCULATOR->aborted()) break;
									if(mpz_divisible_ui_p(cval, (unsigned long int) (root == 2 ? SQUARE_PRIMES[i] : RAISED_PRIMES[root - 3][i]))) {
										nr *= PRIMES[i];
										o_number /= (root == 2 ? SQUARE_PRIMES[i] : RAISED_PRIMES[root - 3][i]);
										b = true;
										break;
									}
								}
							} else {
								for(size_t i = 0; root == 2 ? (i < NR_OF_SQUARE_PRIMES) : (RAISED_PRIMES[root - 3][i] != 0); i++) {
									if(CALCULATOR->aborted()) break;
									if((root == 2 ? SQUARE_PRIMES[i] : RAISED_PRIMES[root - 3][i]) > val) {
										break;
									} else if(val % (root == 2 ? SQUARE_PRIMES[i] : RAISED_PRIMES[root - 3][i]) == 0) {
										nr *= PRIMES[i];
										o_number /= (root == 2 ? SQUARE_PRIMES[i] : RAISED_PRIMES[root - 3][i]);
										b = true;
										break;
									}
								}
							}
						}
						if(!nr.isOne()) {
							transform(STRUCT_MULTIPLICATION);
							CHILD(0).calculateRaise(mstruct, eo, this, 0);
							PREPEND(nr);
							if(!mstruct.number().numeratorIsOne()) {
								CHILD(0).calculateRaise(mstruct.number().numerator(), eo, this, 0);
							}
							calculateMultiplyIndex(0, eo, true, mparent, index_this);
							return 1;
						}
					}
					if(eo.split_squares && nr_root != 2) {
						// partial roots, e.g. 9^(1/4)=3^(1/2)
						if(nr_root.isEven()) {
							Number nr(o_number);
							if(nr.sqrt() && !nr.isApproximate()) {
								o_number = nr;
								mstruct.number().multiply(2);
								mstruct.ref();
								raise_nocopy(&mstruct);
								calculateRaiseExponent(eo, mparent, index_this);
								return 1;
							}
						}
						for(size_t i = 1; i < NR_OF_PRIMES; i++) {
							if(nr_root.isLessThanOrEqualTo(PRIMES[i])) break;
							if(nr_root.isIntegerDivisible(PRIMES[i])) {
								Number nr(o_number);
								if(nr.root(Number(PRIMES[i], 1)) && !nr.isApproximate()) {
									o_number = nr;
									mstruct.number().multiply(PRIMES[i]);
									mstruct.ref();
									raise_nocopy(&mstruct);
									calculateRaiseExponent(eo, mparent, index_this);
									return 1;
								}
							}
						}
					}
				}
			}
		}
		// If base numerator is larger than denominator, invert base and negate exponent
		if(o_number.isRational() && !o_number.isInteger() && !o_number.isZero() && ((o_number.isNegative() && o_number.isGreaterThan(nr_minus_one) && mstruct.number().isInteger()) || (o_number.isPositive() && o_number.isLessThan(nr_one)))) {
			mstruct.number().negate();
			o_number.recip();
			return 0;
		}
		return -1;
	}
	if(mstruct.isOne()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	} else if(isOne() && mstruct.representsNumber()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite(false)) {
		if(mstruct.representsNegative(false)) {
			o_number.clear();
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		} else if(mstruct.representsNonZero(false) && mstruct.representsPositive(false)) {
			if(o_number.isMinusInfinity()) {
				if(mstruct.representsEven(false)) {
					o_number.setPlusInfinity();
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				} else if(mstruct.representsOdd(false)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			} else if(o_number.isPlusInfinity()) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite(false)) {
		if(m_type == STRUCT_VARIABLE && o_variable->isKnown() && ((KnownVariable*) o_variable)->get().isNumber()) {
			if(((KnownVariable*) o_variable)->get().number().isGreaterThan(1) || ((KnownVariable*) o_variable)->get().number().isLessThan(1)) {
				if(mstruct.number().isPlusInfinity()) {
					if(mparent) {
						mparent->swapChildren(index_this + 1, index_mstruct + 1);
					} else {
						set_nocopy(mstruct, true);
					}
					return 1;
				} else if(mstruct.number().isMinusInfinity()) {
					clear(true);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			} else if(((KnownVariable*) o_variable)->get().number().isNonZero() || ((KnownVariable*) o_variable)->get().number().isFraction()) {
				if(mstruct.number().isPlusInfinity()) {
					clear(true);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				} else if(mstruct.number().isMinusInfinity()) {
					if(mparent) {
						mparent->swapChildren(index_this + 1, index_mstruct + 1);
					} else {
						set_nocopy(mstruct, true);
					}
					return 1;
				}
			}
		}
	}
	if(representsUndefined() || mstruct.representsUndefined()) return -1;
	if(isZero() && mstruct.representsPositive()) {
		return 1;
	}
	if(mstruct.isZero() && !representsUndefined(true, true)) {
		set(m_one);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			if(mstruct.isNumber() && mstruct.number().isInteger()) {
				if(isMatrix()) {
					if(matrixIsSquare()) {
						Number nr(mstruct.number());
						bool b_neg = false;
						if(nr.isNegative()) {
							nr.setNegative(false);
							b_neg = true;
						}
						if(!nr.isOne()) {
							MathStructure msave(*this);
							nr--;
							while(nr.isPositive()) {
								if(CALCULATOR->aborted()) {
									set(msave);
									return -1;
								}
								calculateMultiply(msave, eo);
								nr--;
							}
						}
						if(b_neg) {
							invertMatrix(eo);
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					return -1;
				} else {
					if(mstruct.number().isMinusOne()) {
						return -1;
					}
					Number nr(mstruct.number());
					if(nr.isNegative()) {
						nr++;
					} else {
						nr--;
					}
					MathStructure msave(*this);
					calculateMultiply(msave, eo);
					calculateRaise(nr, eo);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				
			}
			goto default_power_merge;
		}
		case STRUCT_ADDITION: {
			if(mstruct.isNumber() && mstruct.number().isInteger()) {
				if(eo.reduce_divisions && mstruct.number().isMinusOne()) {
					int bnum = -1, bden = -1;
					int inegs = 0;
					bool b_break = false;
					for(size_t i = 0; i < SIZE && !b_break; i++) {
						switch(CHILD(i).type()) {
							case STRUCT_NUMBER: {
								if(!CHILD(i).number().isRational() || CHILD(i).number().numeratorIsGreaterThan(1000000L) || CHILD(i).number().numeratorIsLessThan(-1000000L) || CHILD(i).number().denominatorIsGreaterThan(1000L)) {
									bnum = 0; bden = 0; inegs = 0; b_break = true;
								}
								if(bden != 0 && !CHILD(i).number().isInteger()) bden = 1;
								if(bnum != 0 && !CHILD(i).isZero()) {
									if(CHILD(i).number().numeratorIsOne() || CHILD(i).number().numeratorIsMinusOne()) bnum = 0;
									else bnum = 1;
								}
								if(CHILD(i).number().isNegative()) {
									inegs++;
									if(i == 0) inegs++;
								} else if(!CHILD(i).number().isZero()) inegs--;
								break;
							}
							case STRUCT_MULTIPLICATION: {
								if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
									if(!CHILD(i)[0].number().isRational() || CHILD(i)[0].number().numeratorIsGreaterThan(1000000L) || CHILD(i)[0].number().numeratorIsLessThan(-1000000L) || CHILD(i)[0].number().denominatorIsGreaterThan(1000L)) {
										bnum = 0; bden = 0; inegs = 0; b_break = true;
									}
									if(bden != 0 && !CHILD(i)[0].number().isInteger()) bden = 1;
									if(bnum != 0 && !CHILD(i)[0].isZero()) {
										if(CHILD(i)[0].number().numeratorIsOne() || CHILD(i)[0].number().numeratorIsMinusOne()) bnum = 0;
										else bnum = 1;
									}
									if(CHILD(i)[0].number().isNegative()) {
										inegs++;
										if(i == 0) inegs++;
									} else if(!CHILD(i)[0].number().isZero()) inegs--;
									break;
								}
							}
							default: {
								bnum = 0;
								inegs--;
								break;
							}
						}
					}
					if(bden < 0) bden = 0;
					if(bnum < 0) bnum = 0;
					if(bnum || bden) {
						Number nr_num, nr_den(1, 1, 0);
						for(size_t i = 0; i < SIZE && !nr_den.isZero(); i++) {
							switch(CHILD(i).type()) {
								case STRUCT_NUMBER: {
									if(CHILD(i).number().isInteger()) {
										if(bnum && !nr_num.isOne() && !CHILD(i).number().isZero()) {
											if(nr_num.isZero()) nr_num = CHILD(i).number();
											else nr_num.gcd(CHILD(i).number());
										}
									} else {
										if(bnum && !nr_num.isOne() && !CHILD(i).number().isZero()) {
											if(nr_num.isZero()) nr_num = CHILD(i).number().numerator();
											else nr_num.gcd(CHILD(i).number().numerator());
										}
										if(bden) {
											nr_den.lcm(CHILD(i).number().denominator());
											if(nr_den.isGreaterThan(1000000L)) nr_den.clear();
										}
									}
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
										if(CHILD(i)[0].number().isInteger()) {
											if(bnum && !nr_num.isOne() && !CHILD(i)[0].number().isZero()) {
												if(nr_num.isZero()) nr_num = CHILD(i)[0].number();
												else nr_num.gcd(CHILD(i)[0].number());
											}
										} else {
											if(bnum && !nr_num.isOne() && !CHILD(i)[0].number().isZero()) {
												if(nr_num.isZero()) nr_num = CHILD(i)[0].number().numerator();
												else nr_num.gcd(CHILD(i)[0].number().numerator());
											}
											if(bden) {
												nr_den.lcm(CHILD(i)[0].number().denominator());
												if(nr_den.isGreaterThan(1000000L)) nr_den.clear();
											}
										}
										break;
									}
								}
								default: {
									break;
								}
							}
						}
						if(!nr_den.isZero() && (!nr_den.isOne() || !nr_num.isOne())) {
							Number nr(nr_den);
							nr.divide(nr_num);
							nr.setNegative(inegs > 0);
							for(size_t i = 0; i < SIZE; i++) {
								switch(CHILD(i).type()) {
									case STRUCT_NUMBER: {
										CHILD(i).number() *= nr;
										break;
									}
									case STRUCT_MULTIPLICATION: {
										if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
											CHILD(i)[0].number() *= nr;
											CHILD(i).calculateMultiplyIndex(0, eo, true, this, i);
											break;
										}
									}
									default: {
										CHILD(i).calculateMultiply(nr, eo);
									}
								}
							}
							calculatesub(eo, eo, false);
							mstruct.ref();
							raise_nocopy(&mstruct);
							calculateRaiseExponent(eo);
							calculateMultiply(nr, eo, mparent, index_this);
							return 1;
						}
					}
					if(inegs > 0) {
						for(size_t i = 0; i < SIZE; i++) {
							switch(CHILD(i).type()) {
								case STRUCT_NUMBER: {CHILD(i).number().negate(); break;}
								case STRUCT_MULTIPLICATION: {
									if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
										CHILD(i)[0].number().negate();
										CHILD(i).calculateMultiplyIndex(0, eo, true, this, i);
										break;
									}
								}
								default: {
									CHILD(i).calculateNegate(eo);
								}
							}
						}
						mstruct.ref();
						raise_nocopy(&mstruct);
						negate();
						return 1;
					}
				} else if(eo.expand != 0 && !mstruct.number().isZero() && (eo.expand > -2 || !containsInterval())) {
					bool b = true;
					bool neg = mstruct.number().isNegative();
					Number m(mstruct.number());
					m.setNegative(false);
					if(eo.expand == -1 && SIZE > 1) {
						Number num_max;
						switch(SIZE) {
							case 14: {}
							case 13: {}
							case 12: {}
							case 11: {}
							case 10: {num_max.set(2, 1, 0); break;}
							case 9: {}
							case 8: {num_max.set(3, 1, 0); break;}
							case 7: {num_max.set(4, 1, 0); break;}
							case 6: {num_max.set(5, 1, 0); break;}
							case 5: {num_max.set(7, 1, 0); break;}
							case 4: {num_max.set(12, 1, 0); break;}
							case 3: {num_max.set(28, 1, 0); break;}
							case 2: {num_max.set(150, 1, 0); break;}
							default: {
								b = false;
								break;
							}
						}
						if(b && m.isGreaterThan(num_max)) {
							b = false;
						}
					}
					if(b) {
						if(!representsNonMatrix()) {
							MathStructure mthis(*this);
							while(!m.isOne()) {
								if(CALCULATOR->aborted()) {
									set(mthis);
									goto default_power_merge;
								}
								calculateMultiply(mthis, eo);
								m--;
							}
						} else {
							MathStructure mstruct1(CHILD(0));
							MathStructure mstruct2(CHILD(1));
							for(size_t i = 2; i < SIZE; i++) {
								if(CALCULATOR->aborted()) goto default_power_merge;
								mstruct2.add(CHILD(i), true);
							}
							Number k(1);
							Number p1(m);
							Number p2(1);
							p1--;
							Number bn;
							MathStructure msave(*this);
							CLEAR
							APPEND(mstruct1);
							CHILD(0).calculateRaise(m, eo);
							while(k.isLessThan(m)) {
								if(CALCULATOR->aborted() || !bn.binomial(m, k)) {
									set(msave);
									goto default_power_merge;
								}
								APPEND_NEW(bn);
								LAST.multiply(mstruct1);
								if(!p1.isOne()) {
									LAST[1].raise_nocopy(new MathStructure(p1));
									LAST[1].calculateRaiseExponent(eo);
								}
								LAST.multiply(mstruct2, true);
								if(!p2.isOne()) {
									LAST[2].raise_nocopy(new MathStructure(p2));
									LAST[2].calculateRaiseExponent(eo);
								}
								LAST.calculatesub(eo, eo, false);
								k++;
								p1--;
								p2++;
							}
							APPEND(mstruct2);
							LAST.calculateRaise(m, eo);
							calculatesub(eo, eo, false);
						}
						if(neg) calculateInverse(eo);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
			}
			goto default_power_merge;
		}
		case STRUCT_MULTIPLICATION: {
			if(mstruct.representsInteger()) {
				// (xy)^a=x^a*y^a
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculateRaise(mstruct, eo);
				}
				MERGE_APPROX_AND_PREC(mstruct)
				calculatesub(eo, eo, false, mparent, index_this);
				return 1;
			} else {
				// (-5xy)^z=5^2*x^z*(-y)^z && x >= 0 && y<0
				MathStructure mnew;
				mnew.setType(STRUCT_MULTIPLICATION);
				for(size_t i = 0; i < SIZE;) {
					if(CHILD(i).representsNonNegative(true)) {
						CHILD(i).ref();
						mnew.addChild_nocopy(&CHILD(i));
						ERASE(i);
					} else if(CHILD(i).isNumber() && CHILD(i).number().isNegative() && !CHILD(i).number().isMinusOne()) {
						// (-5)^z=5^z*(-1)^z
						CHILD(i).number().negate();
						mnew.addChild(CHILD(i));
						CHILD(i).number().set(-1, 1, 0);
						i++;
					} else {
						i++;
					}
				}
				if(mnew.size() > 0) {
					if(SIZE > 0) {
						if(SIZE == 1) SET_CHILD_MAP(0)
						mnew.addChild(*this);
					}
					set_nocopy(mnew, true);
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).calculateRaise(mstruct, eo);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					calculatesub(eo, eo, false, mparent, index_this);
					return 1;
				}
			}
			goto default_power_merge;
		}
		case STRUCT_POWER: {
			if((eo.allow_complex && CHILD(1).representsFraction()) || (mstruct.representsInteger() && (eo.allow_complex || CHILD(0).representsInteger())) || representsNonNegative(true)) {
				if((((!eo.assume_denominators_nonzero || eo.warn_about_denominators_assumed_nonzero) && !CHILD(0).representsNonZero(true)) || CHILD(0).isZero()) && CHILD(1).representsNegative(true) && CHILD(1).representsNegative(true)) {
					if(!eo.assume_denominators_nonzero || CHILD(0).isZero() || !warn_about_denominators_assumed_nonzero(CHILD(0), eo)) break;
				}
				if(!CHILD(1).representsNonInteger() && !mstruct.representsInteger()) {
					if(CHILD(1).representsEven() && CHILD(0).representsReal(true)) {
						if(CHILD(0).representsNegative(true)) {
							CHILD(0).calculateNegate(eo);
						} else if(!CHILD(0).representsNonNegative(true)) {
							MathStructure mstruct_base(CHILD(0));
							CHILD(0).set(CALCULATOR->f_abs, &mstruct_base, NULL);
						}
					} else if(!CHILD(1).representsOdd() && !CHILD(0).representsNonNegative(true)) {
						goto default_power_merge;
					}
				}
				mstruct.ref();
				MERGE_APPROX_AND_PREC(mstruct)
				CHILD(1).multiply_nocopy(&mstruct, true);
				CHILD(1).calculateMultiplyLast(eo, true, this, 1);
				calculateRaiseExponent(eo, mparent, index_this);
				return 1;
			}
			goto default_power_merge;
		}
		case STRUCT_VARIABLE: {
			if(o_variable == CALCULATOR->v_e) {
				if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
					if(mstruct[0].isNumber()) {
						if(mstruct[0].number().isI() || mstruct[0].number().isMinusI()) {
							//e^(i*pi)=-1
							set(m_minus_one, true);
							return 1;
						} else if(mstruct[0].number().hasImaginaryPart() && !mstruct[0].number().hasRealPart()) {
							Number img(mstruct[0].number().imaginaryPart());
							// e^(a*i*pi)=+-1; e^(a/2*i*pi)=+-1 (a is integer)
							if(img.isInteger()) {
								if(img.isEven()) {
									set(m_one, true);
								} else {
									set(m_minus_one, true);
								}
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							} else if(img.isRational() && img.denominatorIsTwo()) {
								img.floor();
								clear(true);
								if(img.isEven()) {
									img.set(1, 1, 0);
								} else {
									img.set(-1, 1, 0);
								}
								o_number.setImaginaryPart(img);
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							}
						}
					}
				} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_ln && mstruct.size() == 1) {
					if(mstruct[0].representsNumber() && (eo.allow_infinite || mstruct[0].representsNonZero())) {
						// e^ln(x)=x; x!=0
						set_nocopy(mstruct[0], true);
						return 1;
					}
				}
			}
			goto default_power_merge;
		}
		case STRUCT_FUNCTION: {
			if(eo.protected_function != o_function) {
				if(o_function == CALCULATOR->f_abs && SIZE == 1) {
					if(mstruct.representsEven() && CHILD(0).representsReal(true)) {
						// abs(x)^2=x^2
						SET_CHILD_MAP(0);
						mstruct.ref();
						raise_nocopy(&mstruct);
						calculateRaiseExponent(eo);
						return 1;
					}
				} else if(o_function == CALCULATOR->f_signum && CHILD(0).representsReal(true) && SIZE == 2 && ((CHILD(1).isZero() && mstruct.representsPositive()) || CHILD(1).isOne())) {
					if(mstruct.representsOdd()) {
						// sgn(x)^3=sgn(x)
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else if(mstruct.representsEven()) {
						if(CHILD(1).isOne() && CHILD(0).representsReal(true)) {
							SET_CHILD_MAP(0)
							return 1;
						} else {
							// sgn(x)^2=sgn(abs(x))
							CHILD(0).transform(CALCULATOR->f_abs);
							CHILD_UPDATED(0)
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
				} else if(o_function == CALCULATOR->f_root && THIS_VALID_ROOT) {
					if(mstruct.representsEven() && CHILD(0).representsReal(true) && CHILD(1).number().isOdd()) {
						// root(x, 3)^2=abs(x)^(3/2)
						CHILD(0).transform(STRUCT_FUNCTION);
						CHILD(0).setFunction(CALCULATOR->f_abs);
						CHILD(1).number().recip();
						m_type = STRUCT_POWER;
						mstruct.ref();
						raise_nocopy(&mstruct);
						calculateRaiseExponent(eo);
						return 1;
					} else if(mstruct.isNumber() && mstruct.number().isInteger() && !mstruct.number().isMinusOne()) {
						if(mstruct == CHILD(1)) {
							// root(x, a)^a=x
							SET_CHILD_MAP(0)
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						} else if(mstruct.number().isIntegerDivisible(CHILD(1).number())) {
							// root(x, a)^(2a)=x^2
							mstruct.calculateDivide(CHILD(1).number(), eo);
							mstruct.ref();
							SET_CHILD_MAP(0)
							raise_nocopy(&mstruct);
							return 1;
						} else if(CHILD(1).number().isIntegerDivisible(mstruct.number())) {
							// root(x, 3a)^(a)=root(x, 3)
							Number nr(CHILD(1).number());
							if(nr.divide(mstruct.number())) {
								CHILD(1) = nr;
								CHILD_UPDATED(1)
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							}
						}
					}
				}
			}
			goto default_power_merge;
		}
		default: {
			default_power_merge:
			if(mstruct.isAddition()) {
				bool b = representsNonNegative(true);
				if(!b) {
					b = true;
					bool bneg = representsNegative(true);
					for(size_t i = 0; i < mstruct.size(); i++) {
						if(!mstruct[i].representsInteger() && (!bneg || !eo.allow_complex || !mstruct[i].isNumber() || !mstruct[i].number().isRational() || !mstruct[i].number().denominatorIsEven())) {
							b = false;
							break;
						}
					}
				}
				if(b) {
					MathStructure msave(*this);
					clear(true);
					m_type = STRUCT_MULTIPLICATION;
					MERGE_APPROX_AND_PREC(mstruct)
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND(msave);
						mstruct[i].ref();
						LAST.raise_nocopy(&mstruct[i]);
						LAST.calculateRaiseExponent(eo);
						calculateMultiplyLast(eo, false);
					}
					if(SIZE == 1) {
						setToChild(1, false, mparent, index_this + 1);
					} else if(SIZE == 0) {
						clear(true);
					} else {
						evalSort();
					}				
					return 1;
				}
			} else if(mstruct.isMultiplication() && mstruct.size() > 1) {
				bool b = representsNonNegative(true);
				if(!b) {
					b = true;
					for(size_t i = 0; i < mstruct.size(); i++) {
						if(!mstruct[i].representsInteger()) {
							b = false;
							break;
						}
					}
				}
				if(b) {
					MathStructure mthis(*this);
					for(size_t i = 0; i < mstruct.size(); i++) {
						if(isZero() && !mstruct[i].representsPositive(true)) continue;
						if(i == 0) mthis.raise(mstruct[i]);
						else mthis[1] = mstruct[i];
						EvaluationOptions eo2 = eo;
						eo2.split_squares = false;
						// avoid abs(x)^(2y) loop
						if(mthis.calculateRaiseExponent(eo2) && (!mthis.isPower() || !isFunction() || o_function != CALCULATOR->f_abs || SIZE != 1 || CHILD(0) != mthis[0])) {
							set(mthis);
							if(mstruct.size() == 2) {
								if(i == 0) {
									mstruct[1].ref();
									raise_nocopy(&mstruct[1]);
								} else {
									mstruct[0].ref();
									raise_nocopy(&mstruct[0]);
								}
							} else {
								mstruct.ref();
								raise_nocopy(&mstruct);
								CHILD(1).delChild(i + 1);
							}
							calculateRaiseExponent(eo);
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
				}
			} else if(mstruct.isNumber() && mstruct.number().isRational() && !mstruct.number().isInteger() && !mstruct.number().numeratorIsOne() && !mstruct.number().numeratorIsMinusOne()) {
				if(representsNonNegative(true) && (m_type != STRUCT_FUNCTION || o_function != CALCULATOR->f_abs)) {
					bool b;
					if(mstruct.number().isNegative()) {
						b = calculateRaise(-mstruct.number().numerator(), eo);
						if(!b) {
							setToChild(1);
							break;
						}
						raise(m_minus_one);
						CHILD(1).number() /= mstruct.number().denominator();
					} else {
						b = calculateRaise(mstruct.number().numerator(), eo);
						if(!b) {
							setToChild(1);
							break;
						}
						raise(m_one);
						CHILD(1).number() /= mstruct.number().denominator();
					}
					if(b) calculateRaiseExponent(eo);
					return 1;
				}
			}
			break;
		}
	}
	return -1;
}

int MathStructure::merge_logical_and(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this, size_t index_mstruct, bool) {

	if(equals(mstruct)) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	}
	if(mstruct.representsPositive()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	}
	if(mstruct.representsNonPositive()) {
		if(isZero()) return 2;
		clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 3;
	}
	if(representsPositive()) {
		if(mparent) {
			mparent->swapChildren(index_this + 1, index_mstruct + 1);
		} else {
			set_nocopy(mstruct, true);
		}
		return 3;
	}
	if(representsNonPositive()) {
		if(!isZero()) clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	}
	if(isLogicalOr()) {
		MERGE_APPROX_AND_PREC(mstruct)
		if(mstruct.isLogicalOr()) {
			for(size_t i = 0; i < SIZE; ) {
				MathStructure msave(CHILD(i));
				for(size_t i2 = 0; i2 < mstruct.size(); i2++) {
					if(i2 > 0) {
						insertChild(msave, i + 1);
					}
					CHILD(i).calculateLogicalAnd(mstruct[i2], eo, this, i);
					i++;
				}
			}
		} else {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).calculateLogicalAnd(mstruct, eo, this, i);
			}
		}
		calculatesub(eo, eo, false);
		return 1;
	} else if(mstruct.isLogicalOr()) {
		MathStructure mthis(*this);
		MERGE_APPROX_AND_PREC(mstruct)
		for(size_t i = 0; i < mstruct.size(); i++) {
			mstruct[i].ref();
			if(i == 0) {				
				add_nocopy(&mstruct[i], OPERATION_LOGICAL_AND, true);
				calculateLogicalAndLast(eo, true);
			} else {
				add(mthis, OPERATION_LOGICAL_OR, true);
				LAST.add_nocopy(&mstruct[i], OPERATION_LOGICAL_AND, true);
				LAST.calculateLogicalAndLast(eo, true, this, SIZE - 1);
			}
		}
		calculatesub(eo, eo, false);
		return 1;
	} else if(isComparison() && mstruct.isComparison()) {
		if(CHILD(0) == mstruct[0]) {
			ComparisonResult cr = mstruct[1].compare(CHILD(1));
			ComparisonType ct1 = ct_comp, ct2 = mstruct.comparisonType();
			switch(cr) {
				case COMPARISON_RESULT_NOT_EQUAL: {
					if(ct_comp == COMPARISON_EQUALS && ct2 == COMPARISON_EQUALS) {
						clear(true);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else if(ct_comp == COMPARISON_EQUALS && ct2 == COMPARISON_NOT_EQUALS) {
						MERGE_APPROX_AND_PREC(mstruct)
						return 2;
					} else if(ct_comp == COMPARISON_NOT_EQUALS && ct2 == COMPARISON_EQUALS) {
						if(mparent) {
							mparent->swapChildren(index_this + 1, index_mstruct + 1);
						} else {
							set_nocopy(mstruct, true);
						}
						return 3;
					}
					return -1;
				}
				case COMPARISON_RESULT_EQUAL: {
					MERGE_APPROX_AND_PREC(mstruct)
					if(ct_comp == ct2) return 1;
					if(ct_comp == COMPARISON_NOT_EQUALS) {
						if(ct2 == COMPARISON_LESS || ct2 == COMPARISON_EQUALS_LESS) {
							ct_comp = COMPARISON_LESS;
							if(ct2 == COMPARISON_LESS) return 3;
							return 1;
						} else if(ct2 == COMPARISON_GREATER || ct2 == COMPARISON_EQUALS_GREATER) {							
							ct_comp = COMPARISON_GREATER;
							if(ct2 == COMPARISON_GREATER) return 3;
							return 1;
						}
					} else if(ct2 == COMPARISON_NOT_EQUALS) {
						if(ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
							if(ct_comp == COMPARISON_LESS) return 2;
							ct_comp = COMPARISON_LESS;
							return 1;
						} else if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) {
							if(ct_comp == COMPARISON_GREATER) return 2;
							ct_comp = COMPARISON_GREATER;
							return 1;
						}
					} else if((ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_EQUALS) && (ct2 == COMPARISON_EQUALS_LESS || ct2 == COMPARISON_EQUALS_GREATER || ct2 == COMPARISON_EQUALS)) {
						if(ct_comp == COMPARISON_EQUALS) return 2;
						ct_comp = COMPARISON_EQUALS;
						if(ct2 == COMPARISON_EQUALS) return 3;
						return 1;
					} else if((ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) && (ct2 == COMPARISON_LESS || ct2 == COMPARISON_EQUALS_LESS)) {
						if(ct_comp == COMPARISON_LESS) return 2;
						ct_comp = COMPARISON_LESS;
						if(ct2 == COMPARISON_LESS) return 3;
						return 1;
					} else if((ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) && (ct2 == COMPARISON_GREATER || ct2 == COMPARISON_EQUALS_GREATER)) {
						if(ct_comp == COMPARISON_GREATER) return 2;
						ct_comp = COMPARISON_GREATER;
						if(ct2 == COMPARISON_GREATER) return 3;
						return 1;
					}
					clear(true);
					return 1;
				}
				case COMPARISON_RESULT_EQUAL_OR_GREATER: {
					switch(ct1) {
						case COMPARISON_GREATER: {ct1 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct1 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct1 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct1 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
					switch(ct2) {
						case COMPARISON_GREATER: {ct2 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct2 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct2 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct2 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
				}
				case COMPARISON_RESULT_EQUAL_OR_LESS: {
					switch(ct1) {
						case COMPARISON_LESS: {
							if(ct2 == COMPARISON_GREATER || ct2 == COMPARISON_EQUALS) {
								clear(true);
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							} else if(ct2 == COMPARISON_LESS || ct2 == COMPARISON_EQUALS_LESS || ct2 == COMPARISON_NOT_EQUALS) {
								MERGE_APPROX_AND_PREC(mstruct)
								return 2;
							}
							break;
						}
						case COMPARISON_GREATER: {
							if(ct2 == COMPARISON_GREATER) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
								} else {
									set_nocopy(mstruct, true);
								}
								return 3;
							}
							break;
						} 
						case COMPARISON_EQUALS_LESS: {
							if(ct2 == COMPARISON_EQUALS_LESS) {
								MERGE_APPROX_AND_PREC(mstruct)
								return 2;
							}
							break;
						} 
						case COMPARISON_EQUALS_GREATER: {
							if(ct2 == COMPARISON_EQUALS_GREATER || ct2 == COMPARISON_EQUALS) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
								} else {
									set_nocopy(mstruct, true);
								}
								return 3;
							}
							break;
						}
						case COMPARISON_EQUALS: {
							if(ct2 == COMPARISON_GREATER) {
								clear(true);
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							}
							break;
						}
						case COMPARISON_NOT_EQUALS: {
							if(ct2 == COMPARISON_GREATER) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
								} else {
									set_nocopy(mstruct, true);
								}
								return 3;
							}
							break;
						}
					} 
					break;
				}
				case COMPARISON_RESULT_GREATER: {
					switch(ct1) {
						case COMPARISON_GREATER: {ct1 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct1 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct1 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct1 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
					switch(ct2) {
						case COMPARISON_GREATER: {ct2 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct2 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct2 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct2 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
				}
				case COMPARISON_RESULT_LESS: {
					switch(ct1) {
						case COMPARISON_EQUALS: {
							switch(ct2) {
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) clear(true); return 1;}
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {MERGE_APPROX_AND_PREC(mstruct) return 2;}								
								default: {}
							}
							break;
						}
						case COMPARISON_NOT_EQUALS: {
							switch(ct2) {
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {
									if(mparent) {
										mparent->swapChildren(index_this + 1, index_mstruct + 1);
									} else {
										set_nocopy(mstruct, true);
									}
									return 3;
								}
								default: {}
							}
							break;
						}
						case COMPARISON_EQUALS_LESS: {}
						case COMPARISON_LESS: {
							switch(ct2) {
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) clear(true); return 1;}
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
							}
							break;
						}
						case COMPARISON_EQUALS_GREATER: {}
						case COMPARISON_GREATER: {
							switch(ct2) {
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {									 
									if(mparent) {
										mparent->swapChildren(index_this + 1, index_mstruct + 1);
									} else {
										set_nocopy(mstruct, true);
									}
									return 3;
								}
								default: {}
							}
							break;
						}
					}
					break;
				}
				default: {
					return -1;
				}
			}
		} else if(comparisonType() == COMPARISON_EQUALS && !CHILD(0).isNumber() && !CHILD(0).containsInterval() && CHILD(1).isNumber() && mstruct.contains(CHILD(0))) {
			mstruct.replace(CHILD(0), CHILD(1));
			mstruct.calculatesub(eo, eo, true);
			mstruct.ref();
			add_nocopy(&mstruct, OPERATION_LOGICAL_AND);
			calculateLogicalAndLast(eo);
			return 1;
		} else if(mstruct.comparisonType() == COMPARISON_EQUALS && !mstruct[0].isNumber() && !mstruct[0].containsInterval() && mstruct[1].isNumber() && contains(mstruct[0])) {
			replace(mstruct[0], CHILD(1));
			calculatesub(eo, eo, true);
			mstruct.ref();
			add_nocopy(&mstruct, OPERATION_LOGICAL_AND);
			calculateLogicalAndLast(eo);
			return 1;
		}
	} else if(isLogicalAnd()) {
		if(mstruct.isLogicalAnd()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				APPEND_REF(&mstruct[i]);
			}
			MERGE_APPROX_AND_PREC(mstruct)
			calculatesub(eo, eo, false);
		} else {
			APPEND_REF(&mstruct);
			MERGE_APPROX_AND_PREC(mstruct)
			calculatesub(eo, eo, false);			
		}
		return 1;
	} else if(mstruct.isLogicalAnd()) {
		transform(STRUCT_LOGICAL_AND);
		for(size_t i = 0; i < mstruct.size(); i++) {
			APPEND_REF(&mstruct[i]);
		}
		MERGE_APPROX_AND_PREC(mstruct)
		calculatesub(eo, eo, false);
		return 1;
	}
	return -1;

}

int MathStructure::merge_logical_or(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this, size_t index_mstruct, bool) {

	if(mstruct.representsPositive()) {
		if(isOne()) {
			MERGE_APPROX_AND_PREC(mstruct)
			return 2;
		}
		set(1, 1, 0, true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 3;
	}
	if(mstruct.representsNonPositive()) {
		if(representsNonPositive() && !isZero()) clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;			
	}
	if(representsPositive()) {
		if(!isOne()) set(1, 1, 0, true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 2;
	}
	if(representsNonPositive()) {
		if(mparent) {
			mparent->swapChildren(index_this + 1, index_mstruct + 1);
		} else {
			set_nocopy(mstruct, true);
		}
		return 3;
	}
	if(equals(mstruct)) {
		return 2;
	}

	if(isLogicalAnd()) {
		if(mstruct.isLogicalAnd()) {
			if(SIZE < mstruct.size()) {
				bool b = true;
				for(size_t i = 0; i < SIZE; i++) {
					b = false;
					for(size_t i2 = 0; i2 < mstruct.size(); i2++) {
						if(CHILD(i) == mstruct[i2]) {
							b = true;
							break;
						}
					}
					if(!b) break;
				}
				if(b) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 2;
				}
			} else if(SIZE > mstruct.size()) {
				bool b = true;
				for(size_t i = 0; i < mstruct.size(); i++) {
					b = false;
					for(size_t i2 = 0; i2 < SIZE; i2++) {
						if(mstruct[i] == CHILD(i2)) {
							b = true;
							break;
						}
					}
					if(!b) break;
				}
				if(b) {
					if(mparent) {
						mparent->swapChildren(index_this + 1, index_mstruct + 1);
					} else {
						set_nocopy(mstruct, true);
					}
					return 3;
				}
			}
		} else {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i) == mstruct) {
					if(mparent) {
						mparent->swapChildren(index_this + 1, index_mstruct + 1);
					} else {
						set_nocopy(mstruct, true);
					}
					return 3;
				}
			}
		}
	} else if(mstruct.isLogicalAnd()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(equals(mstruct[i])) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 2;
			}
		}
	}

	if(isComparison() && mstruct.isComparison()) {
		if(CHILD(0) == mstruct[0]) {
			ComparisonResult cr = mstruct[1].compare(CHILD(1));
			ComparisonType ct1 = ct_comp, ct2 = mstruct.comparisonType();
			switch(cr) {
				case COMPARISON_RESULT_NOT_EQUAL: {
					return -1;
				}
				case COMPARISON_RESULT_EQUAL: {
					if(ct_comp == ct2) return 1;
					switch(ct_comp) {
						case COMPARISON_EQUALS: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_EQUALS_GREATER: {ct_comp = ct2; MERGE_APPROX_AND_PREC(mstruct) return 3;}
								case COMPARISON_LESS: {ct_comp = COMPARISON_EQUALS_LESS; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_GREATER: {ct_comp = COMPARISON_EQUALS_GREATER; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								default: {}
							}
							break;
						}
						case COMPARISON_NOT_EQUALS: {
							switch(ct2) {
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_EQUALS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}								
								case COMPARISON_LESS: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
								default: {}
							}
							break;
						}
						case COMPARISON_EQUALS_LESS: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_GREATER: {}
								case COMPARISON_EQUALS_GREATER: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS: {}
								case COMPARISON_LESS: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
								default: {}
							}
							break;
						}
						case COMPARISON_LESS: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {ct_comp = ct2; MERGE_APPROX_AND_PREC(mstruct) return 3;}
								case COMPARISON_EQUALS_GREATER: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS: {ct_comp = COMPARISON_EQUALS_LESS; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_GREATER: {ct_comp = COMPARISON_NOT_EQUALS; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								default: {}
							}
							break;
						}
						case COMPARISON_EQUALS_GREATER: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_LESS: {}
								case COMPARISON_EQUALS_LESS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
								default: {}
							}
							break;
						}
						case COMPARISON_GREATER: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {ct_comp = ct2; MERGE_APPROX_AND_PREC(mstruct) return 3;}
								case COMPARISON_EQUALS_LESS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS: {ct_comp = COMPARISON_EQUALS_GREATER; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_LESS: {ct_comp = COMPARISON_NOT_EQUALS; MERGE_APPROX_AND_PREC(mstruct) return 1;}
								default: {}
							}
							break;
						}
					}
					break;
				}
				case COMPARISON_RESULT_EQUAL_OR_GREATER: {
					switch(ct1) {
						case COMPARISON_GREATER: {ct1 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct1 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct1 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct1 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
					switch(ct2) {
						case COMPARISON_GREATER: {ct2 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct2 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct2 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct2 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
				}
				case COMPARISON_RESULT_EQUAL_OR_LESS: {
					switch(ct1) {
						case COMPARISON_LESS: {
							if(ct2 == COMPARISON_LESS || ct2 == COMPARISON_EQUALS_LESS || ct2 == COMPARISON_NOT_EQUALS) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
								} else {
									set_nocopy(mstruct, true);
								}
								return 3;
							}
							break;
						}
						case COMPARISON_GREATER: {
							if(ct2 == COMPARISON_GREATER) {
								MERGE_APPROX_AND_PREC(mstruct)
								return 2;
							}
							break;
						} 
						case COMPARISON_EQUALS_LESS: {
							if(ct2 == COMPARISON_EQUALS_LESS) {
								if(mparent) {
									mparent->swapChildren(index_this + 1, index_mstruct + 1);
								} else {
									set_nocopy(mstruct, true);
								}
								return 3;
							}
							break;
						} 
						case COMPARISON_EQUALS_GREATER: {
							if(ct2 == COMPARISON_EQUALS_GREATER || ct2 == COMPARISON_EQUALS) {
								MERGE_APPROX_AND_PREC(mstruct)
								return 2;
							}
							break;
						}
						case COMPARISON_NOT_EQUALS: {
							if(ct2 == COMPARISON_GREATER) {
								MERGE_APPROX_AND_PREC(mstruct)
								return 2;
							}
							break;
						}
						default: {}
					} 
					break;
				}
				case COMPARISON_RESULT_GREATER: {
					switch(ct1) {
						case COMPARISON_GREATER: {ct1 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct1 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct1 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct1 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
					switch(ct2) {
						case COMPARISON_GREATER: {ct2 = COMPARISON_LESS; break;}
						case COMPARISON_EQUALS_GREATER: {ct2 = COMPARISON_EQUALS_LESS; break;}
						case COMPARISON_LESS: {ct2 = COMPARISON_GREATER; break;}
						case COMPARISON_EQUALS_LESS: {ct2 = COMPARISON_EQUALS_GREATER; break;}
						default: {}
					}
				}
				case COMPARISON_RESULT_LESS: {
					switch(ct1) {
						case COMPARISON_EQUALS: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {
									if(mparent) {
										mparent->swapChildren(index_this + 1, index_mstruct + 1);
									} else {
										set_nocopy(mstruct, true);
									}
									return 3;
								}
								default: {}
							}
							break;
						}
						case COMPARISON_NOT_EQUALS: {
							switch(ct2) {
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								default: {}
							}
							break;
						}
						case COMPARISON_EQUALS_LESS: {}
						case COMPARISON_LESS: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {
									if(mparent) {
										mparent->swapChildren(index_this + 1, index_mstruct + 1);
									} else {
										set_nocopy(mstruct, true);
									}
									return 3;
								}
								default: {}
							}
							break;
						}
						case COMPARISON_EQUALS_GREATER: {}
						case COMPARISON_GREATER: {
							switch(ct2) {
								case COMPARISON_NOT_EQUALS: {}
								case COMPARISON_EQUALS_LESS: {}
								case COMPARISON_LESS: {set(1, 1, 0, true); MERGE_APPROX_AND_PREC(mstruct) return 1;}
								case COMPARISON_EQUALS: {}
								case COMPARISON_EQUALS_GREATER: {}
								case COMPARISON_GREATER: {MERGE_APPROX_AND_PREC(mstruct) return 2;}
							}
							break;
						}
					}
					break;
				}
				default: {
					return -1;
				}
			}
		} else if(comparisonType() == COMPARISON_NOT_EQUALS && !CHILD(0).isNumber() && !CHILD(0).containsInterval() && CHILD(1).isNumber() && mstruct.contains(CHILD(0))) {
			mstruct.replace(CHILD(0), CHILD(1));
			mstruct.calculatesub(eo, eo, true);
			mstruct.ref();
			add_nocopy(&mstruct, OPERATION_LOGICAL_OR);
			calculateLogicalOrLast(eo);
			return 1;
		} else if(mstruct.comparisonType() == COMPARISON_NOT_EQUALS && !mstruct[0].isNumber() && !mstruct[0].containsInterval() && mstruct[1].isNumber() && contains(mstruct[0])) {
			replace(mstruct[0], CHILD(1));
			calculatesub(eo, eo, true);
			mstruct.ref();
			add_nocopy(&mstruct, OPERATION_LOGICAL_OR);
			calculateLogicalOrLast(eo);
			return 1;
		}
	} else if(isLogicalOr()) {
		if(mstruct.isLogicalOr()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				APPEND_REF(&mstruct[i]);
			}
			MERGE_APPROX_AND_PREC(mstruct)
			calculatesub(eo, eo, false);
		} else {
			APPEND_REF(&mstruct);
			MERGE_APPROX_AND_PREC(mstruct)
			calculatesub(eo, eo, false);			
		}
		return 1;
	} else if(mstruct.isLogicalOr()) {
		transform(STRUCT_LOGICAL_OR);
		for(size_t i = 0; i < mstruct.size(); i++) {
			APPEND_REF(&mstruct[i]);
		}
		MERGE_APPROX_AND_PREC(mstruct)
		calculatesub(eo, eo, false);
		return 1;
	}
	return -1;

}

int MathStructure::merge_logical_xor(MathStructure &mstruct, const EvaluationOptions&, MathStructure*, size_t, size_t, bool) {

	if(equals(mstruct)) {
		clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	bool bp1 = representsPositive();
	bool bp2 = mstruct.representsPositive();
	if(bp1 && bp2) {
		clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	bool bn1 = representsNonPositive();	
	bool bn2 = mstruct.representsNonPositive();	
	if(bn1 && bn2) {
		clear(true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	if((bn1 && bp2) || (bp1 && bn2)) {
		set(1, 1, 0, true);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	
	return -1;

}


int MathStructure::merge_bitwise_and(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure*, size_t, size_t, bool) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitAnd(mstruct.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						mstruct[i].ref();
						CHILD(i).add_nocopy(&mstruct[i], OPERATION_LOGICAL_AND);
						CHILD(i).calculatesub(eo, eo, false);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_BITWISE_AND: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return -1;
				}
				case STRUCT_BITWISE_AND: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND_REF(&mstruct[i]);
					}
					calculatesub(eo, eo, false);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					APPEND_REF(&mstruct);
					calculatesub(eo, eo, false);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			break;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_BITWISE_AND: {
					return 0;
				}
				default: {}
			}	
		}
	}
	return -1;
}
int MathStructure::merge_bitwise_or(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure*, size_t, size_t, bool) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitOr(mstruct.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						mstruct[i].ref();
						CHILD(i).add_nocopy(&mstruct[i], OPERATION_LOGICAL_OR);
						CHILD(i).calculatesub(eo, eo, false);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_BITWISE_OR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return -1;
				}
				case STRUCT_BITWISE_OR: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND_REF(&mstruct[i]);
					}
					calculatesub(eo, eo, false);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					APPEND_REF(&mstruct);
					calculatesub(eo, eo, false);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			break;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_BITWISE_OR: {
					return 0;
				}
				default: {}
			}	
		}
	}
	return -1;
}
int MathStructure::merge_bitwise_xor(MathStructure &mstruct, const EvaluationOptions &eo, MathStructure*, size_t, size_t, bool) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitXor(mstruct.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mstruct.number().includesInfinity())) {
			if(o_number == nr) {
				o_number = nr;
				numberUpdated();
				return 2;
			}
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						mstruct[i].ref();
						CHILD(i).add_nocopy(&mstruct[i], OPERATION_LOGICAL_XOR);
						CHILD(i).calculatesub(eo, eo, false);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		default: {}
	}
	return -1;
}

#define MERGE_RECURSE			if(recursive) {\
						bool large_size = (SIZE > 100); \
						for(size_t i = 0; i < SIZE; i++) {\
							if(large_size && CALCULATOR->aborted()) break;\
							if(!CHILD(i).isNumber()) CHILD(i).calculatesub(eo, feo, true, this, i);\
						}\
						CHILDREN_UPDATED;\
					}

#define MERGE_ALL(FUNC, TRY_LABEL) 	size_t i2, i3 = SIZE;\
					bool large_size = (i3 > 100); \
					for(size_t i = 0; i < SIZE - 1; i++) {\
						if(large_size && CALCULATOR->aborted()) break;\
						i2 = i + 1;\
						TRY_LABEL:\
						for(; i2 < i; i2++) {\
							if(large_size && CALCULATOR->aborted()) break;\
							int r = CHILD(i2).FUNC(CHILD(i), eo, this, i2, i);\
							if(r == 0) {\
								SWAP_CHILDREN(i2, i);\
								r = CHILD(i2).FUNC(CHILD(i), eo, this, i2, i, true);\
								if(r < 1) {\
									SWAP_CHILDREN(i2, i);\
								}\
							}\
							if(r >= 1) {\
								ERASE(i);\
								b = true;\
								i3 = i;\
								i = i2;\
								i2 = 0;\
								goto TRY_LABEL;\
							}\
						}\
						for(i2 = i + 1; i2 < SIZE; i2++) {\
							if(large_size && CALCULATOR->aborted()) break;\
							int r = CHILD(i).FUNC(CHILD(i2), eo, this, i, i2);\
							if(r == 0) {\
								SWAP_CHILDREN(i, i2);\
								r = CHILD(i).FUNC(CHILD(i2), eo, this, i, i2, true);\
								if(r < 1) {\
									SWAP_CHILDREN(i, i2);\
								} else if(r == 2) {\
									r = 3;\
								} else if(r == 3) {\
									r = 2;\
								}\
							}\
							if(r >= 1) {\
								ERASE(i2);\
								b = true;\
								if(r != 2) {\
									i2 = 0;\
									goto TRY_LABEL;\
								}\
								i2--;\
							}\
						}\
						if(i3 < SIZE) {\
							if(i3 == SIZE - 1) break;\
							i = i3;\
							i3 = SIZE;\
							i2 = i + 1;\
							goto TRY_LABEL;\
						}\
					}
					
#define MERGE_ALL2			if(SIZE == 1) {\
						setToChild(1, false, mparent, index_this + 1);\
					} else if(SIZE == 0) {\
						clear(true);\
					} else {\
						evalSort();\
					}

bool do_simplification(MathStructure &mstruct, const EvaluationOptions &eo, bool combine_divisions = true, bool only_gcd = false, bool combine_only = false, bool recursive = true, bool limit_size = false);

bool do_simplification(MathStructure &mstruct, const EvaluationOptions &eo, bool combine_divisions, bool only_gcd, bool combine_only, bool recursive, bool limit_size) {

	if(!eo.expand || !eo.assume_denominators_nonzero) return false;

	if(recursive) {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			b = do_simplification(mstruct[i], eo, combine_divisions, only_gcd || (!mstruct.isComparison() && !mstruct.isLogicalAnd() && !mstruct.isLogicalOr()), combine_only, true, limit_size) || b;
			if(CALCULATOR->aborted()) return b;
		}
		if(b) mstruct.calculatesub(eo, eo, false);
		return do_simplification(mstruct, eo, combine_divisions, only_gcd, combine_only, false, limit_size) || b;
	}
	if(mstruct.isPower() && mstruct[1].isNumber() && mstruct[1].number().isRational() && !mstruct[1].number().isInteger() && mstruct[0].isAddition() && mstruct[0].isRationalPolynomial()) {
		MathStructure msqrfree(mstruct[0]);
		if(sqrfree(msqrfree, eo) && msqrfree.isPower() && msqrfree.calculateRaise(mstruct[1], eo)) {
			mstruct = msqrfree;
			return true;
		}
	} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct) && mstruct[0].isAddition() && mstruct[0].isRationalPolynomial()) {
		MathStructure msqrfree(mstruct[0]);
		if(sqrfree(msqrfree, eo) && msqrfree.isPower() && msqrfree[1].isInteger() && msqrfree[1].number().isPositive()) {
			if(msqrfree[1] == mstruct[1]) {
				if(msqrfree[1].number().isEven()) {
					if(!msqrfree[0].representsReal(true)) return false;
					msqrfree.delChild(2);
					msqrfree.setType(STRUCT_FUNCTION);
					msqrfree.setFunction(CALCULATOR->f_abs);
					mstruct = msqrfree;
				} else {
					mstruct = msqrfree[0];
				}
				return true;
			} else if(msqrfree[1].number().isIntegerDivisible(mstruct[1].number())) {
				if(msqrfree[1].number().isEven()) {
					if(!msqrfree[0].representsReal(true)) return false;
					msqrfree[0].transform(STRUCT_FUNCTION);
					msqrfree[0].setFunction(CALCULATOR->f_abs);
				}
				msqrfree[1].number().divide(mstruct[1].number());
				mstruct = msqrfree;
				mstruct.calculatesub(eo, eo, false);
				return true;
			} else if(mstruct[1].number().isIntegerDivisible(msqrfree[1].number())) {
				if(msqrfree[1].number().isEven()) {
					if(!msqrfree[0].representsReal(true)) return false;
					msqrfree[0].transform(STRUCT_FUNCTION);
					msqrfree[0].setFunction(CALCULATOR->f_abs);
				}
				Number new_root(mstruct[1].number());
				new_root.divide(msqrfree[1].number());
				mstruct[0] = msqrfree[0];
				mstruct[1] = new_root;
				return true;
			}
		}
	}
	if(!mstruct.isAddition()) return false;

	if(combine_divisions) {

		MathStructure divs, nums, numleft, mleft;
		
		EvaluationOptions eo2 = eo;
		eo2.do_polynomial_division = false;
		eo2.keep_zero_units = false;

		// find division by polynomial
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(CALCULATOR->aborted()) return false;
			bool b = false;
			if(mstruct[i].isMultiplication()) {
				MathStructure div, num;
				for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
					if(mstruct[i][i2].isPower() && mstruct[i][i2][1].isInteger() && mstruct[i][i2][1].number().isNegative()) {
						bool b_rat = mstruct[i][i2][0].isRationalPolynomial();
						if(!b_rat && mstruct[i][i2][0].isAddition() && mstruct[i][i2][0].size() > 1) {
							b_rat = true;
							for(size_t i3 = 0; i3 < mstruct[i][i2][0].size(); i3++) {
								if(!mstruct[i][i2][0][i3].representsZero(true) && !mstruct[i][i2][0][i3].isRationalPolynomial()) {
									b_rat = false;
									break;
								}
							}
						}
						if(!b_rat) {
							div.clear();
							break;
						}
						bool b_minone = mstruct[i][i2][1].isMinusOne();
						if(b_minone) {
							if(div.isZero()) div = mstruct[i][i2][0];
							else div.multiply(mstruct[i][i2][0], true);
						} else {
							mstruct[i][i2][1].number().negate();
							if(div.isZero()) div = mstruct[i][i2];
							else div.multiply(mstruct[i][i2], true);
							mstruct[i][i2][1].number().negate();
						}
					} else if(mstruct[i][i2].isRationalPolynomial() || mstruct[i][i2].representsZero(true)) {
						if(num.isZero()) num = mstruct[i][i2];
						else num.multiply(mstruct[i][i2], true);
					} else {
						div.clear();
						break;
					}
				}
				if(!div.isZero()) {
					bool b_found = false;
					for(size_t i3 = 0; i3 < divs.size(); i3++) {
						if(divs[i3] == div) {
							if(!num.representsZero(true)) nums[i3].add(num, true);
							b_found = true;
							b = true;
							break;
						}
					}
					if(!b_found && (eo.assume_denominators_nonzero || div.representsNonZero(true)) && !div.representsZero(true)) {
						if(!num.representsZero(true)) {
							divs.addChild(div);
							nums.addChild(num);
						}
						b = true;
					}
				}
			} else if(mstruct[i].isPower() && mstruct[i][1].isInteger() && mstruct[i][1].number().isNegative()) {
				bool b_rat = mstruct[i][0].isRationalPolynomial();
				if(!b_rat && mstruct[i][0].isAddition() && mstruct[i][0].size() > 1) {
					b_rat = true;
					for(size_t i2 = 0; i2 < mstruct[i][0].size(); i2++) {
						if(!mstruct[i][0][i2].representsZero(true) && !mstruct[i][0].isRationalPolynomial()) {
							b_rat = false;
							break;
						}
					}
				}
				if(b_rat) {
					bool b_minone = mstruct[i][1].isMinusOne();
					if(!b_minone) mstruct[i][1].number().negate();
					bool b_found = false;
					for(size_t i3 = 0; i3 < divs.size(); i3++) {
						if((b_minone && divs[i3] == mstruct[i][0]) || (!b_minone && divs[i3] == mstruct[i])) {
							nums[i3].add(m_one, true);
							b_found = true;
							b = true;
							break;
						}
					}
					if(!b_found && (eo.assume_denominators_nonzero || mstruct[i][0].representsNonZero(true)) && !mstruct[i][0].representsZero(true)) {
						if(b_minone) divs.addChild(mstruct[i][0]);
						else divs.addChild(mstruct[i]);
						nums.addChild(m_one);
						b = true;
					}
					if(!b_minone) mstruct[i][1].number().negate();
				}
			}
			if(!b) {
				if(mstruct[i].isRationalPolynomial()) numleft.addChild(mstruct[i]);
				else mleft.addChild(mstruct[i]);
			}
		}
		
		for(size_t i = 0; i < divs.size(); i++) {
			if(divs[i].isAddition()) {
				for(size_t i2 = 0; i2 < divs[i].size();) {
					if(divs[i][i2].representsZero(true)) {
						divs[i].delChild(i2 + 1);
					} else i2++;
				}
				if(divs[i].size() == 1) divs[i].setToChild(1);
				else if(divs[i].size() == 0) divs[i].clear();
			}
		}

		if(divs.size() == 0) return false;
		
		bool b_ret = false;
		if(divs.size() > 1 || numleft.size() > 0) b_ret = true;
		while(divs.size() > 0) {
			bool b = true;
			if(!divs[0].isRationalPolynomial() || !nums[0].isRationalPolynomial()) {
				return false;
			}
			if(!combine_only && divs[0].isAddition() && nums[0].isAddition()) {
				MathStructure ca, cb, mgcd;
				if(MathStructure::gcd(nums[0], divs[0], mgcd, eo2, &ca, &cb, false) && !mgcd.isOne() && (!cb.isAddition() || !ca.isAddition() || ca.size() + cb.size() <= nums[0].size() + divs[0].size())) {
					if(mgcd.representsNonZero(true) || !eo.warn_about_denominators_assumed_nonzero || warn_about_denominators_assumed_nonzero(mgcd, eo)) {
						if(cb.isOne()) {
							numleft.addChild(ca);
							nums.delChild(1);
							divs.delChild(1);
							b = false;
						} else {
							nums[0] = ca;
							divs[0] = cb;
						}
						b_ret = true;
					}
				}
			}
			if(CALCULATOR->aborted()) return false;
			if(b && divs.size() > 1) {
				if(!combine_only && divs[1].isAddition() && nums[1].isAddition()) {
					MathStructure ca, cb, mgcd;
					if(MathStructure::gcd(nums[1], divs[1], mgcd, eo2, &ca, &cb, false) && !mgcd.isOne() && (!cb.isAddition() || !ca.isAddition() || ca.size() + cb.size() <= nums[1].size() + divs[1].size())) {
						if(mgcd.representsNonZero(true) || !eo.warn_about_denominators_assumed_nonzero || warn_about_denominators_assumed_nonzero(mgcd, eo)) {
							if(cb.isOne()) {
								numleft.addChild(ca);
								nums.delChild(2);
								divs.delChild(2);
								b = false;
							} else {
								nums[1] = ca;
								divs[1] = cb;
							}
						}
					}
				}
				if(CALCULATOR->aborted()) return false;
				if(b) {
					MathStructure ca, cb, mgcd;
					b = MathStructure::gcd(divs[0], divs[1], mgcd, eo2, &ca, &cb, false) && !mgcd.isOne();
					if(CALCULATOR->aborted()) return false;
					bool b_merge = true;
					if(b) {
						if(limit_size && ((cb.isAddition() && ((divs[0].isAddition() && divs[0].size() * cb.size() > 200) || (nums[0].isAddition() && nums[0].size() * cb.size() > 200))) || (ca.isAddition() && nums[1].isAddition() && nums[1].size() * ca.size() > 200))) {
							b_merge = false;
						} else {
							if(!cb.isOne()) {
								divs[0].calculateMultiply(cb, eo2);
								nums[0].calculateMultiply(cb, eo2);
							}
							if(!ca.isOne()) {
								nums[1].calculateMultiply(ca, eo2);
							}
						}
					} else {
						if(limit_size && ((divs[1].isAddition() && ((divs[0].isAddition() && divs[0].size() * divs[1].size() > 200) || (nums[0].isAddition() && nums[0].size() * divs[1].size() > 200))) || (divs[0].isAddition() && nums[1].isAddition() && nums[1].size() * divs[0].size() > 200))) {
							b_merge = false;
						} else {
							nums[0].calculateMultiply(divs[1], eo2);
							nums[1].calculateMultiply(divs[0], eo2);
							divs[0].calculateMultiply(divs[1], eo2);
						}
					}
					if(b_merge) {
						nums[0].calculateAdd(nums[1], eo2);
						nums.delChild(2);
						divs.delChild(2);
					} else {
						size_t size_1 = 2, size_2 = 2;
						if(nums[0].isAddition()) size_1 += nums[0].size() - 1;
						if(divs[0].isAddition()) size_1 += divs[0].size() - 1;
						if(nums[1].isAddition()) size_2 += nums[1].size() - 1;
						if(divs[1].isAddition()) size_2 += divs[1].size() - 1;
						if(size_1 > size_2) {
							nums[0].calculateDivide(divs[0], eo);
							mleft.addChild(nums[0]);
							nums.delChild(1);
							divs.delChild(1);
						} else {
							nums[1].calculateDivide(divs[1], eo);
							mleft.addChild(nums[1]);
							nums.delChild(2);
							divs.delChild(2);
						}
					}
				}
			} else if(b && numleft.size() > 0) {
				if(limit_size && divs[0].isAddition() && numleft.size() > 0 && divs[0].size() * numleft.size() > 200) break;
				if(numleft.size() == 1) numleft.setToChild(1);
				else if(numleft.size() > 1) numleft.setType(STRUCT_ADDITION);
				numleft.calculateMultiply(divs[0], eo2);
				nums[0].calculateAdd(numleft, eo2);
				numleft.clear();
			} else if(b) break;
		}
		if(CALCULATOR->aborted()) return false;
		if(!combine_only && !only_gcd && divs.size() > 0 && nums[0].isAddition() && divs[0].isAddition()) {
			MathStructure mquo, mrem;
			if(polynomial_long_division(nums[0], divs[0], m_zero, mquo, mrem, eo2, false) && !mquo.isZero() && mrem != nums[0]) {
				if(CALCULATOR->aborted()) return false;
				if(!mrem.isZero() || divs[0].representsNonZero(true) || (eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(divs[0], eo))) {
					if(mrem.isZero()) {
						mleft.addChild(mquo);
						divs.clear();
						b_ret = true;
					} else {
						long int point = nums[0].size();
						if(mquo.isAddition()) point -= mquo.size();
						else point--;
						if(mrem.isAddition()) point -= mrem.size();
						else point--;
						if(point >= 0) {
							mleft.addChild(mquo);
							MathStructure ca, cb, mgcd;
							if(mrem.isAddition() && MathStructure::gcd(mrem, divs[0], mgcd, eo2, &ca, &cb, false) && !mgcd.isOne() && (!cb.isAddition() || !ca.isAddition() || ca.size() + cb.size() <= mrem.size() + divs[0].size())) {
								mrem = ca;
								divs[0] = cb;
							}
							mrem.calculateDivide(divs[0], eo2);
							mleft.addChild(mrem);
							divs.clear();
							b_ret = true;
						}
					}
				}
			}
		}
		if(!b_ret) return false;
		mstruct.clear(true);
		if(divs.size() > 0) {
			mstruct = nums[0];
			if(only_gcd || combine_only) {
				divs[0].inverse();
				mstruct.multiply(divs[0], true);
			} else {
				mstruct.calculateDivide(divs[0], eo);
			}
		}
		for(size_t i = 0; i < mleft.size(); i++) {
			if(i == 0 && mstruct.isZero()) mstruct = mleft[i];
			else mstruct.calculateAdd(mleft[i], eo);
		}
		for(size_t i = 0; i < numleft.size(); i++) {
			if(i == 0 && mstruct.isZero()) mstruct = numleft[i];
			else mstruct.calculateAdd(numleft[i], eo);
		}
		return true;
	}
	
	MathStructure divs;
	// find division by polynomial
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(mstruct[i].isMultiplication()) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				if(mstruct[i][i2].isPower() && (mstruct[i][i2][0].isAddition() || combine_divisions) && mstruct[i][i2][1].isMinusOne() && mstruct[i][i2][0].isRationalPolynomial()) {
					bool b_found = false;
					for(size_t i3 = 0; i3 < divs.size(); i3++) {
						if(divs[i3] == mstruct[i][i2][0]) {
							divs[i3].number()++;
							b_found = true;
							break;
						}
					}
					if(!b_found) divs.addChild(mstruct[i][i2][0]);
					break;
				}
			}
		} else if(mstruct[i].isPower() && (mstruct[i][0].isAddition() || combine_divisions) && mstruct[i][1].isMinusOne() && mstruct[i][0].isRationalPolynomial()) {
			bool b_found = false;
			for(size_t i3 = 0; i3 < divs.size(); i3++) {
				if(divs[i3] == mstruct[i][0]) {
					divs[i3].number()++;
					b_found = true;
					break;
				}
			}
			if(!b_found) divs.addChild(mstruct[i][0]);
		}
	}

	// check if denominators is zero
	for(size_t i = 0; i < divs.size(); ) {
		if((!combine_divisions && divs[i].number().isZero()) || (!eo.assume_denominators_nonzero && !divs[i].representsNonZero(true)) || divs[i].representsZero(true)) divs.delChild(i + 1);
		else i++;
	}

	if(divs.size() == 0) return false;

	// combine numerators with same denominator
	MathStructure nums, numleft, mleft;
	nums.resizeVector(divs.size(), m_zero);
	for(size_t i = 0; i < mstruct.size(); i++) {
		bool b = false;
		if(mstruct[i].isMultiplication()) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				if(mstruct[i][i2].isPower() && (mstruct[i][i2][0].isAddition() || combine_divisions) && mstruct[i][i2][1].isMinusOne() && mstruct[i][i2][0].isRationalPolynomial()) {
					for(size_t i3 = 0; i3 < divs.size(); i3++) {
						if(divs[i3] == mstruct[i][i2][0]) {
							if(mstruct[i].size() == 1) nums[i3].addChild(m_one);
							else if(mstruct[i].size() == 2) nums[i3].addChild(mstruct[i][i2 == 0 ? 1 : 0]);
							else {nums[i3].addChild(mstruct[i]); nums[i3][nums[i3].size() - 1].delChild(i2 + 1);}
							b = true;
							break;
						}
					}
					break;
				}
			}
		} else if(mstruct[i].isPower() && (mstruct[i][0].isAddition() || combine_divisions) && mstruct[i][1].isMinusOne() && mstruct[i][0].isRationalPolynomial()) {
			for(size_t i3 = 0; i3 < divs.size(); i3++) {
				if(divs[i3] == mstruct[i][0]) {
					nums[i3].addChild(m_one);
					b = true;
					break;
				}
			}
		}
		if(!b && combine_divisions) {
			if(mstruct[i].isRationalPolynomial()) numleft.addChild(mstruct[i]);
			else mleft.addChild(mstruct[i]);
		}
	}

	EvaluationOptions eo2 = eo;
	eo2.do_polynomial_division = false;
	eo2.keep_zero_units = false;
	
	// do polynomial division; give points to incomplete division
	vector<long int> points(nums.size(), -1);
	bool b = false, b_ready_candidate = false;
	for(size_t i = 0; i < divs.size(); i++) {
		if(CALCULATOR->aborted()) return false;
		if(nums[i].size() > 1 && divs[i].isAddition()) {
			nums[i].setType(STRUCT_ADDITION);
			MathStructure xvar;
			get_first_symbol(nums[i], xvar);
			if(nums[i].isRationalPolynomial() && nums[i].degree(xvar).isLessThan(100) && divs[i].degree(xvar).isLessThan(100)) {
				MathStructure mquo, mrem, ca, cb;
				if(MathStructure::gcd(nums[i], divs[i], mquo, eo2, &ca, &cb, false) && !mquo.isOne()) {
					if(!mquo.representsNonZero(true) && eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(mquo, eo)) {
						nums[i].clear();
					} else {
						points[i] = 1;
						b_ready_candidate = true;
						b = true;
					}
					if(ca.isOne()) {
						if(cb.isOne()) {
							nums[i].set(1, 1, 0, true);
						} else {
							nums[i] = cb;
							nums[i].inverse();
						}
					} else if(cb.isOne()) {
						nums[i] = ca;
					} else {
						nums[i] = ca;
						nums[i].calculateDivide(cb, eo);
					}
				} else if(!only_gcd && polynomial_long_division(nums[i], divs[i], xvar, mquo, mrem, eo2, false) && !mquo.isZero() && mrem != nums[i]) {
					if(mrem.isZero() && !divs[i].representsNonZero(true) && eo.warn_about_denominators_assumed_nonzero && !warn_about_denominators_assumed_nonzero(divs[i], eo)) {
						nums[i].clear();
					} else {
						long int point = 1;
						if(!mrem.isZero()) point = nums[i].size();
						nums[i].set(mquo);
						if(!mrem.isZero()) {
							if(mquo.isAddition()) point -= mquo.size();
							else point--;
							if(mrem.isAddition()) point -= mrem.size();
							else point--;
							mrem.calculateDivide(divs[i], eo2);
							nums[i].calculateAdd(mrem, eo2);
						}
						b = true;
						points[i] = point;
						if(point >= 0) {b_ready_candidate = true;}
						else if(b_ready_candidate) nums[i].clear();
					}
				} else {
					nums[i].clear();
				}
			}
		} else {
			nums[i].clear();
		}
	}

	if(!b) return false;

	if(b_ready_candidate) {
		// remove polynomial divisions that inrease complexity
		for(size_t i = 0; i < nums.size(); i++) {
			if(!nums[i].isZero() && points[i] < 0) nums[i].clear();
		}
	} else {
		// no simplying polynomial division found; see if result can be combined with other terms
		b = false;
		for(size_t i = 0; i < nums.size(); i++) {
			if(!nums[i].isZero()) {
				if(b) {
					nums[i].clear();
				} else {
					long int point = points[i];
					for(size_t i2 = 0; i2 < nums[i].size(); i2++) {
						bool b2 = false;
						if(!nums[i][i2].contains(divs[i], false, false, false)) {
							MathStructure mtest1(mstruct), mtest2(nums[i][i2]);
							for(size_t i3 = 0; i3 < mtest1.size(); i3++) {
								if(!mtest1[i3].contains(divs[i], false, false, false)) {
									int ret = mtest1[i3].merge_addition(mtest2, eo);
									if(ret > 0) {
										b2 = true;
										point++;
										if(mtest1[i3].isZero()) point++;
										break;
									}
									if(ret == 0) ret = mtest2.merge_addition(mtest1[i3], eo);
									if(ret > 0) {
										b2 = true;
										point++;
										if(mtest2.isZero()) point++;
										break;
									}
								}
							}
							if(b2) break;
						}
						if(point >= 0) break;
					}
					if(point >= 0) b = true;
					else nums[i].clear();
				}
			}
		}
	}

	if(!b) return false;
	// replace terms with polynomial division result
	for(size_t i = 0; i < mstruct.size(); ) {
		bool b_del = false;
		if(mstruct[i].isMultiplication()) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				if(mstruct[i][i2].isPower() && mstruct[i][i2][0].isAddition() && mstruct[i][i2][1].isMinusOne() && mstruct[i][i2][0].isRationalPolynomial()) {
					for(size_t i3 = 0; i3 < divs.size(); i3++) {
						if(divs[i3] == mstruct[i][i2][0]) {
							b_del = !nums[i3].isZero();
							break;
						}
					}
					break;
				}
			}
		} else if(mstruct[i].isPower() && mstruct[i][0].isAddition() && mstruct[i][1].isMinusOne() && mstruct[i][0].isRationalPolynomial()) {
			for(size_t i3 = 0; i3 < divs.size(); i3++) {
				if(divs[i3] == mstruct[i][0]) {
					b_del = !nums[i3].isZero();
					break;
				}
			}
		}
		if(b_del) mstruct.delChild(i + 1);
		else i++;
	}
	for(size_t i = 0; i < nums.size(); ) {
		if(nums[i].isZero()) {
			nums.delChild(i + 1);
		} else {
			nums[i].evalSort();
			i++;
		}
	}
	if(mstruct.size() == 0 && nums.size() == 1) {
		mstruct.set(nums[0]);
	} else {
		for(size_t i = 0; i < nums.size(); i++) {
			mstruct.addChild(nums[i]);
		}
		mstruct.evalSort();
	}
	return true;
}

bool fix_intervals(MathStructure &mstruct, const EvaluationOptions &eo, bool *failed = NULL, long int min_precision = 2) {
	if(mstruct.type() == STRUCT_NUMBER) {
		if(CALCULATOR->usesIntervalArithmetic()) {
			if(!mstruct.number().isInterval(false) && mstruct.number().precision() >= 0) {
				mstruct.number().precisionToInterval();
				mstruct.setPrecision(-1);
				mstruct.numberUpdated();
				return true;
			}
		} else if(mstruct.number().isInterval(false)) {
			if(!mstruct.number().intervalToPrecision(min_precision)) {
				if(failed) *failed = true;
				return false;
			}
			mstruct.numberUpdated();
			return true;
		}
	} else if(mstruct.type() == STRUCT_FUNCTION && mstruct.function() == CALCULATOR->f_interval) {
		bool b = mstruct.calculateFunctions(eo, false);
		if(b) {
			fix_intervals(mstruct, eo, failed);
			return true;
		}
	} else {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(fix_intervals(mstruct[i], eo, failed)) {
				mstruct.childUpdated(i + 1);
				b = true;
			}
		}
		return b;
	}
	return false;
}

bool contains_zero_unit(const MathStructure &mstruct);
bool contains_zero_unit(const MathStructure &mstruct) {
	if(mstruct.isMultiplication() && mstruct.size() > 1 && mstruct[0].isZero()) {
		bool b = true;
		for(size_t i = 1; i < mstruct.size(); i++) {
			if(!mstruct[i].isUnit_exp()) {
				b = false;
				break;
			}
		}
		if(b) return true;
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(contains_zero_unit(mstruct[i])) return true;
	}
	return false;
}

bool MathStructure::calculatesub(const EvaluationOptions &eo, const EvaluationOptions &feo, bool recursive, MathStructure *mparent, size_t index_this) {
	if(b_protected) return false;
	bool b = false;
	switch(m_type) {
		case STRUCT_VARIABLE: {
			if(eo.calculate_variables && o_variable->isKnown()) {
				if((eo.approximation == APPROXIMATION_APPROXIMATE || (!o_variable->isApproximate() && !((KnownVariable*) o_variable)->get().containsInterval()  && !((KnownVariable*) o_variable)->get().containsFunction(CALCULATOR->f_interval))) && !((KnownVariable*) o_variable)->get().isAborted()) {
					set(((KnownVariable*) o_variable)->get());
					if(eo.calculate_functions) {
						calculateFunctions(feo);
					}
					fix_intervals(*this, feo, NULL, PRECISION);
					b = true;
					calculatesub(eo, feo, true, mparent, index_this);
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(recursive) {
				CHILD(0).calculatesub(eo, feo, true, this, 0);
				CHILD(1).calculatesub(eo, feo, true, this, 1);
				CHILDREN_UPDATED;
			}
			if(CHILD(0).merge_power(CHILD(1), eo) >= 1) {
				b = true;
				setToChild(1, false, mparent, index_this + 1);
			}
			break;
		}
		case STRUCT_ADDITION: {
			MERGE_RECURSE
			bool found_complex_relations = false;
			if(eo.sync_units && (syncUnits(false, &found_complex_relations, true, feo) || (found_complex_relations && eo.sync_complex_unit_relations))) {
				if(found_complex_relations && eo.sync_complex_unit_relations) {
					EvaluationOptions eo2 = eo;
					eo2.expand = -3;
					eo2.combine_divisions = false;
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).calculatesub(eo2, feo, true, this, i);
						CHILD(i).factorizeUnits();
					}
					CHILDREN_UPDATED;
					syncUnits(true, NULL, true, feo);
				}
				unformat(eo);
				MERGE_RECURSE
			}
			MERGE_ALL(merge_addition, try_add)
			MERGE_ALL2
			break;
		}
		case STRUCT_MULTIPLICATION: {

			MERGE_RECURSE
			if(eo.sync_units && syncUnits(eo.sync_complex_unit_relations, NULL, true, feo)) {
				unformat(eo);
				MERGE_RECURSE
			}

			if(representsNonMatrix()) {
				if(SIZE > 2 && CALCULATOR->usesIntervalArithmetic()) {
					int nonintervals = 0, had_interval = false;
					for(size_t i = 0; i < SIZE; i++) {
						if(CHILD(i).isNumber()) {
							if(CHILD(i).number().isInterval()) {
								had_interval = true;
								if(nonintervals >= 2) break;
							} else if(nonintervals < 2) {
								nonintervals++;
								if(nonintervals == 2 && had_interval) break;
							}
						}
					}
					if(had_interval && nonintervals >= 2) evalSort(false);
				}
				MERGE_ALL(merge_multiplication, try_multiply)
			} else {
				size_t i2, i3 = SIZE;
				for(size_t i = 0; i < SIZE - 1; i++) {
					i2 = i + 1;
					try_multiply_matrix:
					bool b_matrix = !CHILD(i).representsNonMatrix();
					if(i2 < i) {
						for(; ; i2--) {
							int r = CHILD(i2).merge_multiplication(CHILD(i), eo, this, i2, i);
							if(r == 0) {
								SWAP_CHILDREN(i2, i);
								r = CHILD(i2).merge_multiplication(CHILD(i), eo, this, i2, i, true);
								if(r < 1) {
									SWAP_CHILDREN(i2, i);
								}
							}
							if(r >= 1) {
								ERASE(i);
								b = true;
								i3 = i;
								i = i2;
								i2 = 0;
								goto try_multiply_matrix;
							}
							if(i2 == 0) break;
							if(b_matrix && !CHILD(i2).representsNonMatrix()) break;
						}
					}
					bool had_matrix = false;
					for(i2 = i + 1; i2 < SIZE; i2++) {
						if(had_matrix && !CHILD(i2).representsNonMatrix()) continue;
						int r = CHILD(i).merge_multiplication(CHILD(i2), eo, this, i, i2);
						if(r == 0) {
							SWAP_CHILDREN(i, i2);
							r = CHILD(i).merge_multiplication(CHILD(i2), eo, this, i, i2, true);
							if(r < 1) {
								SWAP_CHILDREN(i, i2);
							} else if(r == 2) {
								r = 3;
							} else if(r == 3) {
								r = 2;
							}
						}
						if(r >= 1) {
							ERASE(i2);
							b = true;
							if(r != 2) {
								i2 = 0;
								goto try_multiply_matrix;
							}
							i2--;
						}
						if(i == SIZE - 1) break;
						if(b_matrix && !CHILD(i2).representsNonMatrix()) had_matrix = true;
					}
					if(i3 < SIZE) {
						if(i3 == SIZE - 1) break;
						i = i3;
						i3 = SIZE;
						i2 = i + 1;
						goto try_multiply_matrix;
					}
				}
			}
			
			MERGE_ALL2

			break;
		}
		case STRUCT_BITWISE_AND: {
			MERGE_RECURSE
			MERGE_ALL(merge_bitwise_and, try_bitand)
			MERGE_ALL2
			break;
		}
		case STRUCT_BITWISE_OR: {
			MERGE_RECURSE
			MERGE_ALL(merge_bitwise_or, try_bitor)
			MERGE_ALL2
			break;
		}
		case STRUCT_BITWISE_XOR: {
			MERGE_RECURSE
			MERGE_ALL(merge_bitwise_xor, try_bitxor)
			MERGE_ALL2
			break;
		}
		case STRUCT_BITWISE_NOT: {
			if(recursive) {
				CHILD(0).calculatesub(eo, feo, true, this, 0);
				CHILDREN_UPDATED;
			}
			switch(CHILD(0).type()) {
				case STRUCT_NUMBER: {
					Number nr(CHILD(0).number());
					if(nr.bitNot() && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || CHILD(0).number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || CHILD(0).number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || CHILD(0).number().includesInfinity())) {
						set(nr, true);
					}
					break;
				}
				case STRUCT_VECTOR: {
					SET_CHILD_MAP(0);
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).setLogicalNot();
					}
					break;
				}
				case STRUCT_BITWISE_NOT: {
					setToChild(1);
					setToChild(1);
					break;
				}
				default: {}
			}
			break;
		}
		case STRUCT_LOGICAL_AND: {			
			if(recursive) {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo, true, this, i);
					CHILD_UPDATED(i)
					if(CHILD(i).representsNonPositive()) {
						clear(true);
						b = true;
						break;
					}
				}
				if(b) break;				
			}
			MERGE_ALL(merge_logical_and, try_logand)
			if(SIZE == 1) {
				if(CHILD(0).representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
					setToChild(1, false, mparent, index_this + 1);
				} else if(CHILD(0).representsPositive()) {
					set(1, 1, 0, true);			
				} else if(CHILD(0).representsNonPositive()) {
					clear(true);
				} else {
					APPEND(m_zero);
					m_type = STRUCT_COMPARISON;
					ct_comp = COMPARISON_GREATER;
				}
			} else if(SIZE == 0) {
				clear(true);
			} else {
				evalSort();
			}
			break;
		}
		case STRUCT_LOGICAL_OR: {
			bool isResistance = false;
			switch (CHILD(0).type()) {
				case STRUCT_MULTIPLICATION: {
					if(CHILD(0)[1] != 0 && CHILD(0)[1].unit() && CHILD(0)[1].unit()->name().find("ohm") != string::npos) {
						isResistance = true;
					}
					break;
				}
				case STRUCT_UNIT: {
					if (CHILD(0).unit() && CHILD(0).unit()->name().find("ohm") != string::npos) {
						isResistance = true;
					}
					break;
				}
				default: {}
			}

			if (isResistance) {
				MathStructure mstruct;
				for (size_t i = 0; i < SIZE; i++) {
					MathStructure mtemp(CHILD(i));
					mtemp.inverse();
					mstruct += mtemp;
				}
				mstruct.inverse();
				clear();
				set(mstruct);
				break;
			}

			if(recursive) {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo, true, this, i);
					CHILD_UPDATED(i)
					if(CHILD(i).representsPositive()) {
						set(1, 1, 0, true);
						b = true;
						break;
					}
				}
				if(b) break;				
			}
			MERGE_ALL(merge_logical_or, try_logor)
			if(SIZE == 1) {
				if(CHILD(0).representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
					setToChild(1, false, mparent, index_this + 1);
				} else if(CHILD(0).representsPositive()) {
					set(1, 1, 0, true);
				} else if(CHILD(0).representsNonPositive()) {
					clear(true);
				} else {
					APPEND(m_zero);
					m_type = STRUCT_COMPARISON;
					ct_comp = COMPARISON_GREATER;
				}
			} else if(SIZE == 0) {
				clear(true);
			} else {
				evalSort();
			}
			break;
		}
		case STRUCT_LOGICAL_XOR: {
			if(recursive) {
				CHILD(0).calculatesub(eo, feo, true, this, 0);
				CHILD(1).calculatesub(eo, feo, true, this, 1);
				CHILDREN_UPDATED;
			}
			if(CHILD(0).merge_logical_xor(CHILD(1), eo) >= 1) {
				b = true;
				setToChild(1, false, mparent, index_this + 1);				
			}
			break;
		}
		case STRUCT_LOGICAL_NOT: {
			if(recursive) {
				CHILD(0).calculatesub(eo, feo, true, this, 0);
				CHILDREN_UPDATED;
			}
			if(CHILD(0).representsPositive()) {
				clear(true);
				b = true;
			} else if(CHILD(0).representsNonPositive()) {
				set(1, 1, 0, true);
				b = true;
			} else if(CHILD(0).isLogicalNot()) {
				setToChild(1);
				setToChild(1);
				if(!representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
					add(m_zero, OPERATION_GREATER);
					calculatesub(eo, feo, false);
				}
				b = true;
			}
			break;
		}
		case STRUCT_COMPARISON: {
			EvaluationOptions eo2 = eo;
			if(eo2.assume_denominators_nonzero == 1) eo2.assume_denominators_nonzero = false;
			if(recursive) {			
				CHILD(0).calculatesub(eo2, feo, true, this, 0);
				CHILD(1).calculatesub(eo2, feo, true, this, 1);
				CHILDREN_UPDATED;
			}
			if(eo.sync_units && syncUnits(eo.sync_complex_unit_relations, NULL, true, feo)) {
				unformat(eo);
				if(recursive) {			
					CHILD(0).calculatesub(eo2, feo, true, this, 0);
					CHILD(1).calculatesub(eo2, feo, true, this, 1);
					CHILDREN_UPDATED;
				}
			}
			if(CHILD(0).isAddition() || CHILD(1).isAddition()) {
				size_t i2 = 0;
				for(size_t i = 0; !CHILD(0).isAddition() || i < CHILD(0).size(); i++) {
					if(CHILD(1).isAddition()) {
						for(; i2 < CHILD(1).size(); i2++) {
							if(CHILD(0).isAddition() && CHILD(0)[i] == CHILD(1)[i2]) {
								CHILD(0).delChild(i + 1);
								CHILD(1).delChild(i2 + 1);
								break;
							} else if(!CHILD(0).isAddition() && CHILD(0) == CHILD(1)[i2]) {
								CHILD(0).clear(true);
								CHILD(1).delChild(i2 + 1);
								break;
							}
						}
					} else if(CHILD(0)[i] == CHILD(1)) {
						CHILD(1).clear(true);
						CHILD(0).delChild(i2 + 1);
						break;
					}
					if(!CHILD(0).isAddition()) break;
				}
				if(CHILD(0).isAddition()) {
					if(CHILD(0).size() == 1) CHILD(0).setToChild(1, true);
					else if(CHILD(0).size() == 0) CHILD(0).clear(true);
				}
				if(CHILD(1).isAddition()) {
					if(CHILD(1).size() == 1) CHILD(1).setToChild(1, true);
					else if(CHILD(1).size() == 0) CHILD(1).clear(true);
				}
			}
			if(CHILD(0).isMultiplication() && CHILD(1).isMultiplication()) {
				size_t i1 = 0, i2 = 0;
				if(CHILD(0)[0].isNumber()) i1++;
				if(CHILD(1)[0].isNumber()) i2++;
				while(i1 < CHILD(0).size() && i2 < CHILD(1).size()) {
					if(CHILD(0)[i1] == CHILD(1)[i2] && CHILD(0)[i1].representsPositive(true)) {
						CHILD(0).delChild(i1 + 1);
						CHILD(1).delChild(i2 + 1);
					} else {
						break;
					}
				}
				if(CHILD(0).size() == 1) CHILD(0).setToChild(1, true);
				else if(CHILD(0).size() == 0) CHILD(0).set(1, 1, 0, true);
				if(CHILD(1).size() == 1) CHILD(1).setToChild(1, true);
				else if(CHILD(1).size() == 0) CHILD(1).set(1, 1, 0, true);
			}
			if(CHILD(0).isNumber() && CHILD(1).isNumber()) {
				ComparisonResult cr = CHILD(1).number().compareApproximately(CHILD(0).number());
				if(cr >= COMPARISON_RESULT_UNKNOWN) {
					break;
				}
				switch(ct_comp) {
					case COMPARISON_EQUALS: {
						if(cr == COMPARISON_RESULT_EQUAL) {
							set(1, 1, 0, true);
							b = true;
						} else if(COMPARISON_IS_NOT_EQUAL(cr)) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_NOT_EQUALS: {
						if(cr == COMPARISON_RESULT_EQUAL) {
							clear(true);
							b = true;
						} else if(COMPARISON_IS_NOT_EQUAL(cr)) {
							set(1, 1, 0, true);
							b = true;
						}
						break;
					}
					case COMPARISON_LESS: {
						if(cr == COMPARISON_RESULT_LESS) {
							set(1, 1, 0, true);
							b = true;
						} else if(cr != COMPARISON_RESULT_EQUAL_OR_LESS && cr != COMPARISON_RESULT_NOT_EQUAL) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_EQUALS_LESS: {
						if(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
							set(1, 1, 0, true);
							b = true;
						} else if(cr != COMPARISON_RESULT_EQUAL_OR_GREATER && cr != COMPARISON_RESULT_NOT_EQUAL) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_GREATER: {
						if(cr == COMPARISON_RESULT_GREATER) {
							set(1, 1, 0, true);
							b = true;
						} else if(cr != COMPARISON_RESULT_EQUAL_OR_GREATER && cr != COMPARISON_RESULT_NOT_EQUAL) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_EQUALS_GREATER: {
						if(COMPARISON_IS_EQUAL_OR_GREATER(cr)) {
							set(1, 1, 0, true);
							b = true;
						} else if(cr != COMPARISON_RESULT_EQUAL_OR_LESS && cr != COMPARISON_RESULT_NOT_EQUAL) {
							clear(true);
							b = true;
						}
						break;
					}
				}
				break;
			}
			if(!eo.test_comparisons) {
				break;
			}
			if(eo2.keep_zero_units && contains_zero_unit(*this)) {
				eo2.keep_zero_units = false;
				MathStructure mtest(*this);
				CALCULATOR->beginTemporaryStopMessages();
				mtest.calculatesub(eo2, feo, true);
				if(mtest.isNumber()) {
					CALCULATOR->endTemporaryStopMessages(true);
					set(mtest);
					b = true;
					break;
				}
				CALCULATOR->endTemporaryStopMessages();
			}
			
			if((CHILD(0).representsUndefined() && !CHILD(1).representsUndefined(true, true, true)) || (CHILD(1).representsUndefined() && !CHILD(0).representsUndefined(true, true, true))) {
				if(ct_comp == COMPARISON_EQUALS) {
					clear(true);
					b = true;
					break;
				} else if(ct_comp == COMPARISON_NOT_EQUALS) {
					set(1, 1, 0, true);
					b = true;
					break;
				}
			}
			if((ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_GREATER) && CHILD(1).isZero()) {
				if(CHILD(0).isLogicalNot() || CHILD(0).isLogicalAnd() || CHILD(0).isLogicalOr() || CHILD(0).isLogicalXor() || CHILD(0).isComparison()) {
					if(ct_comp == COMPARISON_EQUALS_LESS) {
						ERASE(1);
						m_type = STRUCT_LOGICAL_NOT;
						calculatesub(eo, feo, false, mparent, index_this);
					} else {
						setToChild(1, false, mparent, index_this + 1);
					}
					b = true;
				}
			} else if((ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_LESS) && CHILD(0).isZero()) {
				if(CHILD(0).isLogicalNot() || CHILD(1).isLogicalAnd() || CHILD(1).isLogicalOr() || CHILD(1).isLogicalXor() || CHILD(1).isComparison()) {
					if(ct_comp == COMPARISON_EQUALS_GREATER) {
						ERASE(0);
						m_type = STRUCT_LOGICAL_NOT;
						calculatesub(eo, feo, false, mparent, index_this);
					} else {
						setToChild(2, false, mparent, index_this + 1);
					}
					b = true;
				}
			}
			if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
				if((CHILD(0).representsReal(true) && CHILD(1).representsComplex(true)) || (CHILD(1).representsReal(true) && CHILD(0).representsComplex(true))) {
					if(ct_comp == COMPARISON_EQUALS) {
						clear(true);
					} else {
						set(1, 1, 0, true);
					}
					b = true;
				} else if((CHILD(0).representsZero(true) && CHILD(1).representsZero(true))) {
					if(ct_comp != COMPARISON_EQUALS) {
						clear(true);
					} else {
						set(1, 1, 0, true);
					}
					b = true;
				}
			}
			if(b) break;
			
			if(eo.approximation == APPROXIMATION_EXACT && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				eo2.test_comparisons = false;
				MathStructure mtest(*this);
				if(mtest[0].isAddition() && mtest[0].size() > 1 && mtest[1].isZero()) {
					mtest[1] = mtest[0][0];
					mtest[1].negate();
					mtest[0].delChild(1, true);
				}
				CALCULATOR->beginTemporaryStopMessages();
				mtest.calculatesub(eo2, feo, true);
				if(CALCULATOR->endTemporaryStopMessages(NULL, NULL, MESSAGE_ERROR) == 0 && ((ct_comp == COMPARISON_EQUALS && mtest.isZero()) || (ct_comp == COMPARISON_NOT_EQUALS && mtest.isOne()))) {
					if(mtest.isZero()) clear(true);
					else set(1, 1, 0, true);
					b = true;
					break;
				}
			}
			eo2 = eo;
			if(eo2.assume_denominators_nonzero == 1) eo2.assume_denominators_nonzero = false;

			bool mtest_new = false;
			MathStructure *mtest;
			if(!CHILD(1).isZero()) {
				if(!eo.isolate_x || find_x_var().isUndefined()) {
					CHILD(0).calculateSubtract(CHILD(1), eo2);
					CHILD(1).clear();
					mtest = &CHILD(0);
					mtest->ref();
				} else {
					mtest = new MathStructure(CHILD(0));
					mtest->calculateSubtract(CHILD(1), eo2);
					mtest_new = true;
				}
			} else {
				mtest = &CHILD(0);
				mtest->ref();				
			}			
			int incomp = 0;
			if(mtest->isAddition()) {
				mtest->evalSort(true);
				incomp = compare_check_incompability(mtest);
			}
			if(incomp < 0) {				
				if(mtest_new && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
					bool a_pos = CHILD(0).representsPositive(true);
					bool a_nneg = a_pos || CHILD(0).representsNonNegative(true);
					bool a_neg = !a_nneg && CHILD(0).representsNegative(true);
					bool a_npos = !a_pos && (a_neg || CHILD(0).representsNonPositive(true));
					bool b_pos = CHILD(1).representsPositive(true);
					bool b_nneg = b_pos || CHILD(1).representsNonNegative(true);
					bool b_neg = !b_nneg && CHILD(1).representsNegative(true);
					bool b_npos = !b_pos && (b_neg || CHILD(1).representsNonPositive(true));
					if(isApproximate()) {
						if((a_pos && b_neg) || (a_neg && b_pos)) {
							incomp = 1;
						}
					} else {
						if((a_pos && b_npos) || (a_npos && b_pos) || (a_nneg && b_neg) || (a_neg && b_nneg)) {
							incomp = 1;
						}
					}
				} else {
					mtest->unref();
					break;
				}
			}
			
			switch(ct_comp) {
				case COMPARISON_EQUALS: {
					if(incomp > 0) {
						clear(true);
						b = true;
					} else if(mtest->representsZero(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsNonZero(true)) {
						clear(true);
						b = true;
					}
					break;
				}
				case COMPARISON_NOT_EQUALS: {
					if(incomp > 0) {
						set(1, 1, 0, true);
						b = true;
					} else if(mtest->representsNonZero(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsZero(true)) {
						clear(true);
						b = true;
					}
					break;
				}
				case COMPARISON_LESS: {
					if(incomp > 0) {
					} else if(mtest->representsNegative(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsNonNegative(true)) {
						clear(true);
						b = true;
					}
					break;
				}
				case COMPARISON_GREATER: {
					if(incomp > 0) {
					} else if(mtest->representsPositive(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsNonPositive(true)) {
						clear(true);
						b = true;
					}
					break;
				}
				case COMPARISON_EQUALS_LESS: {
					if(incomp > 0) {
					} else if(mtest->representsNonPositive(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsPositive(true)) {
						clear(true);
						b = true;
					}
					break;
				}
				case COMPARISON_EQUALS_GREATER: {
					if(incomp > 0) {
					} else if(mtest->representsNonNegative(true)) {
						set(1, 1, 0, true);
						b = true;	
					} else if(mtest->representsNegative(true)) {
						clear(true);
						b = true;
					}
					break;
				}
			}
			mtest->unref();
			break;
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_abs || o_function == CALCULATOR->f_root || o_function == CALCULATOR->f_interval || o_function == CALCULATOR->f_signum || o_function == CALCULATOR->f_dirac || o_function == CALCULATOR->f_heaviside) {
				b = calculateFunctions(eo, false);
				if(b) {
					calculatesub(eo, feo, true, mparent, index_this);
					break;
				}
			}
		}
		default: {
			if(recursive) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).calculatesub(eo, feo, true, this, i)) b = true;
				}
				CHILDREN_UPDATED;
			}
			if(eo.sync_units && syncUnits(eo.sync_complex_unit_relations, NULL, true, feo)) {
				unformat(eo);
				if(recursive) {
					for(size_t i = 0; i < SIZE; i++) {
						if(CHILD(i).calculatesub(eo, feo, true, this, i)) b = true;
					}
					CHILDREN_UPDATED;
				}
			}
		}
	}
	return b;
}

#define MERGE_INDEX(FUNC, TRY_LABEL)	bool b = false;\
					TRY_LABEL:\
					bool large_size = SIZE > 100;\
					for(size_t i = 0; i < index; i++) {\
						if(large_size && CALCULATOR->aborted()) break; \
						int r = CHILD(i).FUNC(CHILD(index), eo, this, i, index);\
						if(r == 0) {\
							SWAP_CHILDREN(i, index);\
							r = CHILD(i).FUNC(CHILD(index), eo, this, i, index, true);\
							if(r < 1) {\
								SWAP_CHILDREN(i, index);\
							} else if(r == 2) {\
								r = 3;\
							} else if(r == 3) {\
								r = 2;\
							}\
						}\
						if(r >= 1) {\
							ERASE(index);\
							if(!b && r == 2) {\
								b = true;\
								index = SIZE;\
								break;\
							} else {\
								b = true;\
								index = i;\
								goto TRY_LABEL;\
							}\
						}\
					}\
					for(size_t i = index + 1; i < SIZE; i++) {\
						if(large_size && CALCULATOR->aborted()) break; \
						int r = CHILD(index).FUNC(CHILD(i), eo, this, index, i);\
						if(r == 0) {\
							SWAP_CHILDREN(index, i);\
							r = CHILD(index).FUNC(CHILD(i), eo, this, index, i, true);\
							if(r < 1) {\
								SWAP_CHILDREN(index, i);\
							} else if(r == 2) {\
								r = 3;\
							} else if(r == 3) {\
								r = 2;\
							}\
						}\
						if(r >= 1) {\
							ERASE(i);\
							if(!b && r == 3) {\
								b = true;\
								break;\
							}\
							b = true;\
							if(r != 2) {\
								goto TRY_LABEL;\
							}\
							i--;\
						}\
					}

#define MERGE_INDEX2			if(b && check_size) {\
						if(SIZE == 1) {\
							setToChild(1, false, mparent, index_this + 1);\
						} else if(SIZE == 0) {\
							clear(true);\
						} else {\
							evalSort();\
						}\
						return true;\
					} else {\
						evalSort();\
						return b;\
					}


bool MathStructure::calculateMergeIndex(size_t index, const EvaluationOptions &eo, const EvaluationOptions &feo, MathStructure *mparent, size_t index_this) {
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {
			return calculateMultiplyIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_ADDITION: {
			return calculateAddIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_POWER: {
			return calculateRaiseExponent(eo, mparent, index_this);
		}
		case STRUCT_LOGICAL_AND: {
			return calculateLogicalAndIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_LOGICAL_OR: {
			return calculateLogicalOrIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_LOGICAL_XOR: {
			return calculateLogicalXorLast(eo, mparent, index_this);
		}
		case STRUCT_BITWISE_AND: {
			return calculateBitwiseAndIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_BITWISE_OR: {
			return calculateBitwiseOrIndex(index, eo, true, mparent, index_this);
		}
		case STRUCT_BITWISE_XOR: {
			return calculateBitwiseXorIndex(index, eo, true, mparent, index_this);
		}
		default: {}
	}
	return calculatesub(eo, feo, false, mparent, index_this);
}
bool MathStructure::calculateLogicalOrLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateLogicalOrIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateLogicalOrIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isLogicalOr()) {
		CALCULATOR->error(true, "calculateLogicalOrIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_logical_or, try_logical_or_index)
	
	if(b && check_size) {
		if(SIZE == 1) {
			if(CHILD(0).representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
				setToChild(1, false, mparent, index_this + 1);
			} else if(CHILD(0).representsPositive()) {
				clear(true);
				o_number.setTrue();					
			} else if(CHILD(0).representsNonPositive()) {
				clear(true);
				o_number.setFalse();					
			} else {
				APPEND(m_zero);
				m_type = STRUCT_COMPARISON;
				ct_comp = COMPARISON_GREATER;
			}
		} else if(SIZE == 0) {
			clear(true);
		} else {
			evalSort();
		}
		return true;
	} else {
		evalSort();
		return false;
	}
	
}
bool MathStructure::calculateLogicalOr(const MathStructure &mor, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mor, OPERATION_LOGICAL_OR, true);
	LAST.evalSort();
	return calculateLogicalOrIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateLogicalXorLast(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {

	if(!isLogicalXor()) {
		CALCULATOR->error(true, "calculateLogicalXorLast() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}
	if(CHILD(0).merge_logical_xor(CHILD(1), eo, this, 0, 1) >= 1) {
		if(CHILD(0).representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
			setToChild(1, false, mparent, index_this + 1);
		} else if(CHILD(0).representsPositive()) {
			clear(true);
			o_number.setTrue();					
		} else if(CHILD(0).representsNonPositive()) {
			clear(true);
			o_number.setFalse();					
		} else {
			APPEND(m_zero);
			m_type = STRUCT_COMPARISON;
			ct_comp = COMPARISON_GREATER;
		}
		return true;
	}
	return false;
	
}
bool MathStructure::calculateLogicalXor(const MathStructure &mxor, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mxor, OPERATION_LOGICAL_XOR);
	LAST.evalSort();
	return calculateLogicalXorLast(eo, mparent, index_this);
}
bool MathStructure::calculateLogicalAndLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateLogicalAndIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateLogicalAndIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isLogicalAnd()) {
		CALCULATOR->error(true, "calculateLogicalAndIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_logical_and, try_logical_and_index)
	
	if(b && check_size) {
		if(SIZE == 1) {
			if(CHILD(0).representsBoolean() || (mparent && !mparent->isMultiplication() && mparent->representsBoolean())) {
				setToChild(1, false, mparent, index_this + 1);
			} else if(CHILD(0).representsPositive()) {
				clear(true);
				o_number.setTrue();					
			} else if(CHILD(0).representsNonPositive()) {
				clear(true);
				o_number.setFalse();					
			} else {
				APPEND(m_zero);
				m_type = STRUCT_COMPARISON;
				ct_comp = COMPARISON_GREATER;
			}	
		} else if(SIZE == 0) {
			clear(true);
		} else {
			evalSort();
		}
		return true;
	} else {
		evalSort();
		return false;
	}
	
}
bool MathStructure::calculateLogicalAnd(const MathStructure &mand, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mand, OPERATION_LOGICAL_AND, true);
	LAST.evalSort();
	return calculateLogicalAndIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateInverse(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	return calculateRaise(m_minus_one, eo, mparent, index_this);
}
bool MathStructure::calculateNegate(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.negate() && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
		return false;
	}
	if(!isMultiplication()) transform(STRUCT_MULTIPLICATION);
	PREPEND(m_minus_one);
	return calculateMultiplyIndex(0, eo, true, mparent, index_this);
}
bool MathStructure::calculateBitwiseNot(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	transform(STRUCT_LOGICAL_NOT);
	return calculatesub(eo, eo, false, mparent, index_this); 
}
bool MathStructure::calculateLogicalNot(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	transform(STRUCT_BITWISE_NOT);
	return calculatesub(eo, eo, false, mparent, index_this); 
}
bool MathStructure::calculateRaiseExponent(const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(!isPower()) {
		CALCULATOR->error(true, "calculateRaiseExponent() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}
	if(CHILD(0).merge_power(CHILD(1), eo, this, 0, 1) >= 1) {
		setToChild(1, false, mparent, index_this + 1);
		return true;
	}
	return false;
}
bool MathStructure::calculateRaise(const MathStructure &mexp, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(mexp.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.raise(mexp.number(), eo.approximation < APPROXIMATION_APPROXIMATE) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mexp.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mexp.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mexp.number().includesInfinity())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
	}
	raise(mexp);
	LAST.evalSort();
	return calculateRaiseExponent(eo, mparent, index_this);
}
bool MathStructure::calculateBitwiseAndLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateBitwiseAndIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateBitwiseAndIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isBitwiseAnd()) {
		CALCULATOR->error(true, "calculateBitwiseAndIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_bitwise_and, try_bitwise_and_index)
	MERGE_INDEX2
	
}
bool MathStructure::calculateBitwiseAnd(const MathStructure &mand, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mand, OPERATION_BITWISE_AND, true);
	LAST.evalSort();
	return calculateBitwiseAndIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateBitwiseOrLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateBitwiseOrIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateBitwiseOrIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isBitwiseOr()) {
		CALCULATOR->error(true, "calculateBitwiseOrIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_bitwise_or, try_bitwise_or_index)
	MERGE_INDEX2
	
}
bool MathStructure::calculateBitwiseOr(const MathStructure &mor, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mor, OPERATION_BITWISE_OR, true);
	LAST.evalSort();
	return calculateBitwiseOrIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateBitwiseXorLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateBitwiseXorIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateBitwiseXorIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isBitwiseXor()) {
		CALCULATOR->error(true, "calculateBitwiseXorIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_bitwise_xor, try_bitwise_xor_index)
	MERGE_INDEX2
	
}
bool MathStructure::calculateBitwiseXor(const MathStructure &mxor, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	add(mxor, OPERATION_BITWISE_XOR, true);
	LAST.evalSort();
	return calculateBitwiseXorIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateMultiplyLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateMultiplyIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateMultiplyIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {

	if(index >= SIZE || !isMultiplication()) {
		CALCULATOR->error(true, "calculateMultiplyIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	bool b = false;		
	try_multiply_matrix_index:
	bool b_matrix = !CHILD(index).representsNonMatrix();
	bool large_size = (SIZE > 100);
	if(index > 0) {
		for(size_t i = index - 1; ; i--) {
			if(large_size && CALCULATOR->aborted()) break;
			int r = CHILD(i).merge_multiplication(CHILD(index), eo, this, i, index);
			if(r == 0) {
				SWAP_CHILDREN(i, index);
				r = CHILD(i).merge_multiplication(CHILD(index), eo, this, i, index, true);
				if(r < 1) {
					SWAP_CHILDREN(i, index);
				} else if(r == 2) {
					r = 3;
				} else if(r == 3) {
					r = 2;
				}
			}
			if(r >= 1) {
				ERASE(index);
				if(!b && r == 2) {
					b = true;
					index = SIZE;
					break;
				} else {
					b = true;
					index = i;
					goto try_multiply_matrix_index;
				}
			}
			if(i == 0) break;
			if(b_matrix && !CHILD(i).representsNonMatrix()) break;
		}
	}

	bool had_matrix = false;
	for(size_t i = index + 1; i < SIZE; i++) {
		if(had_matrix && !CHILD(i).representsNonMatrix()) continue;
		if(large_size && CALCULATOR->aborted()) break;
		int r = CHILD(index).merge_multiplication(CHILD(i), eo, this, index, i);
		if(r == 0) {
			SWAP_CHILDREN(index, i);
			r = CHILD(index).merge_multiplication(CHILD(i), eo, this, index, i, true);
			if(r < 1) {
				SWAP_CHILDREN(index, i);
			} else if(r == 2) {
				r = 3;
			} else if(r == 3) {
				r = 2;
			}
		}
		if(r >= 1) {
			ERASE(i);
			if(!b && r == 3) {
				b = true;
				break;
			}
			b = true;
			if(r != 2) {
				goto try_multiply_matrix_index;
			}
			i--;
		}
		if(i == SIZE - 1) break;
		if(b_matrix && !CHILD(i).representsNonMatrix()) had_matrix = true;
	}

	MERGE_INDEX2

}
bool MathStructure::calculateMultiply(const MathStructure &mmul, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(mmul.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.multiply(mmul.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mmul.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mmul.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mmul.number().includesInfinity())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
		return false;
	}
	multiply(mmul, true);
	LAST.evalSort();
	return calculateMultiplyIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateDivide(const MathStructure &mdiv, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(mdiv.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.divide(mdiv.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mdiv.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mdiv.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || o_number.includesInfinity() || mdiv.number().includesInfinity())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
		return false;
	}
	MathStructure *mmul = new MathStructure(mdiv);
	mmul->evalSort();
	multiply_nocopy(mmul, true);
	LAST.calculateInverse(eo, this, SIZE - 1);
	return calculateMultiplyIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateAddLast(const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	return calculateAddIndex(SIZE - 1, eo, check_size, mparent, index_this);
}
bool MathStructure::calculateAddIndex(size_t index, const EvaluationOptions &eo, bool check_size, MathStructure *mparent, size_t index_this) {
	
	if(index >= SIZE || !isAddition()) {
		CALCULATOR->error(true, "calculateAddIndex() error: %s. %s", print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}

	MERGE_INDEX(merge_addition, try_add_index)
	MERGE_INDEX2	
	
}
bool MathStructure::calculateAdd(const MathStructure &madd, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(madd.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.add(madd.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || madd.number().isApproximate())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
		return false;
	}
	add(madd, true);
	LAST.evalSort();
	return calculateAddIndex(SIZE - 1, eo, true, mparent, index_this);
}
bool MathStructure::calculateSubtract(const MathStructure &msub, const EvaluationOptions &eo, MathStructure *mparent, size_t index_this) {
	if(msub.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.subtract(msub.number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || msub.number().isApproximate())) {
			o_number = nr;
			numberUpdated();
			return true;
		}
		return false;
	}
	MathStructure *madd = new MathStructure(msub);
	madd->evalSort();	
	add_nocopy(madd, true);
	LAST.calculateNegate(eo, this, SIZE - 1);
	return calculateAddIndex(SIZE - 1, eo, true, mparent, index_this);
}

bool MathStructure::calculateFunctions(const EvaluationOptions &eo, bool recursive, bool do_unformat) {

	if(m_type == STRUCT_FUNCTION && o_function != eo.protected_function) {

		if(function_value) {
			function_value->unref();
			function_value = NULL;
		}

		if(!o_function->testArgumentCount(SIZE)) {
			return false;
		}

		if(o_function->maxargs() > -1 && (long int) SIZE > o_function->maxargs()) {
			REDUCE(o_function->maxargs());
		}
		m_type = STRUCT_VECTOR;
		Argument *arg = NULL, *last_arg = NULL;
		int last_i = 0;

		for(size_t i = 0; i < SIZE; i++) {
			arg = o_function->getArgumentDefinition(i + 1);
			if(arg) {
				last_arg = arg;
				last_i = i;
				if(!arg->test(CHILD(i), i + 1, o_function, eo)) {
					m_type = STRUCT_FUNCTION;
					CHILD_UPDATED(i);
					return false;
				} else {
					CHILD_UPDATED(i);
				}
			}
		}

		if(last_arg && o_function->maxargs() < 0 && last_i >= o_function->minargs()) {
			for(size_t i = last_i + 1; i < SIZE; i++) {
				if(!last_arg->test(CHILD(i), i + 1, o_function, eo)) {
					m_type = STRUCT_FUNCTION;
					CHILD_UPDATED(i);
					return false;
				} else {
					CHILD_UPDATED(i);
				}
			}
		}

		if(!o_function->testCondition(*this)) {
			m_type = STRUCT_FUNCTION;
			return false;
		}
		MathStructure *mstruct = new MathStructure();
		int i = o_function->calculate(*mstruct, *this, eo);
		if(i > 0) {
			set_nocopy(*mstruct, true);
			if(recursive) calculateFunctions(eo);
			mstruct->unref();
			if(do_unformat) unformat(eo);
			return true;
		} else {
			if(i < 0) {
				i = -i;
				if(o_function->maxargs() > 0 && i > o_function->maxargs()) {
					if(mstruct->isVector()) {
						if(do_unformat) mstruct->unformat(eo);
						for(size_t arg_i = 1; arg_i <= SIZE && arg_i <= mstruct->size(); arg_i++) {
							mstruct->getChild(arg_i)->ref();
							setChild_nocopy(mstruct->getChild(arg_i), arg_i);
						}
					}
				} else if(i <= (long int) SIZE) {
					if(do_unformat) mstruct->unformat(eo);
					mstruct->ref();
					setChild_nocopy(mstruct, i);
				}
			}
			/*if(eo.approximation == APPROXIMATION_EXACT) {
				mstruct->clear();
				EvaluationOptions eo2 = eo;
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				CALCULATOR->beginTemporaryStopMessages();
				if(o_function->calculate(*mstruct, *this, eo2) > 0) {
					function_value = mstruct;
					function_value->ref();
					function_value->calculateFunctions(eo2);
				}
				if(CALCULATOR->endTemporaryStopMessages() > 0 && function_value) {
					function_value->unref();
					function_value = NULL;
				}
			}*/
			m_type = STRUCT_FUNCTION;
			mstruct->unref();
			return false;
		}
	}
	bool b = false;
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CALCULATOR->aborted()) break;
			if(CHILD(i).calculateFunctions(eo)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
	}
	return b;

}

int evalSortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, bool b_abs = false);
int evalSortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, bool b_abs) {
	if(parent.isMultiplication()) {
		if((!mstruct1.representsNonMatrix() && !mstruct2.representsScalar()) || (!mstruct2.representsNonMatrix() && !mstruct1.representsScalar())) {
			return 0;
		}
	}
	if(b_abs || parent.isAddition()) {
		if(mstruct1.isMultiplication() && mstruct1.size() > 0) {
			size_t start = 0;
			while(mstruct1[start].isNumber() && mstruct1.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct2.isMultiplication()) {
				if(mstruct2.size() < 1) return -1;
				size_t start2 = 0;
				while(mstruct2[start2].isNumber() && mstruct2.size() > start2 + 1) {
					start2++;
				}
				for(size_t i = 0; ; i++) {
					if(i + start2 >= mstruct2.size()) {
						if(i + start >= mstruct1.size()) {
							if(start2 == start) {
								for(size_t i3 = 0; i3 < start; i3++) {
									i2 = evalSortCompare(mstruct1[i3], mstruct2[i3], parent, b_abs);
									if(i2 != 0) return i2;
								}
								return 0;
							}
							if(start2 > start) return -1;
						}
						return 1;
					}
					if(i + start >= mstruct1.size()) return -1;
					i2 = evalSortCompare(mstruct1[i + start], mstruct2[i + start2], parent, b_abs);
					if(i2 != 0) return i2;
				}
				if(mstruct1.size() - start == mstruct2.size() - start2) return 0;
				return -1;
			} else {
				i2 = evalSortCompare(mstruct1[start], mstruct2, parent, b_abs);
				if(i2 != 0) return i2;
				if(b_abs && start == 1 && (mstruct1[0].isMinusOne() || mstruct1[0].isOne())) return 0;
				return 1;
			}
		} else if(mstruct2.isMultiplication() && mstruct2.size() > 0) {
			size_t start = 0;
			while(mstruct2[start].isNumber() && mstruct2.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct1.isMultiplication()) {
				return 1;
			} else {
				i2 = evalSortCompare(mstruct1, mstruct2[start], parent, b_abs);
				if(i2 != 0) return i2;
				if(b_abs && start == 1 && (mstruct2[0].isMinusOne() || mstruct2[0].isOne())) return 0;
				return -1;
			}
		} 
	}
	if(mstruct1.type() != mstruct2.type()) {
		if(!parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		if(!parent.isMultiplication() || (!mstruct1.isNumber() && !mstruct2.isNumber())) {
			if(mstruct2.isPower()) {
				int i = evalSortCompare(mstruct1, mstruct2[0], parent, b_abs);
				if(i == 0) {
					return evalSortCompare(m_one, mstruct2[1], parent, b_abs);
				}
				return i;
			}
			if(mstruct1.isPower()) {
				int i = evalSortCompare(mstruct1[0], mstruct2, parent, b_abs);
				if(i == 0) {
					return evalSortCompare(mstruct1[1], m_one, parent, b_abs);
				}
				return i;
			}
		}
		if(mstruct2.isAborted()) return -1;
		if(mstruct1.isAborted()) return 1;
		if(mstruct2.isInverse()) return -1;
		if(mstruct1.isInverse()) return 1;
		if(mstruct2.isDivision()) return -1;
		if(mstruct1.isDivision()) return 1;
		if(mstruct2.isNegate()) return -1;
		if(mstruct1.isNegate()) return 1;
		if(mstruct2.isLogicalAnd()) return -1;
		if(mstruct1.isLogicalAnd()) return 1;
		if(mstruct2.isLogicalOr()) return -1;
		if(mstruct1.isLogicalOr()) return 1;
		if(mstruct2.isLogicalXor()) return -1;
		if(mstruct1.isLogicalXor()) return 1;
		if(mstruct2.isLogicalNot()) return -1;
		if(mstruct1.isLogicalNot()) return 1;
		if(mstruct2.isComparison()) return -1;
		if(mstruct1.isComparison()) return 1;
		if(mstruct2.isBitwiseOr()) return -1;
		if(mstruct1.isBitwiseOr()) return 1;
		if(mstruct2.isBitwiseXor()) return -1;
		if(mstruct1.isBitwiseXor()) return 1;
		if(mstruct2.isBitwiseAnd()) return -1;
		if(mstruct1.isBitwiseAnd()) return 1;
		if(mstruct2.isBitwiseNot()) return -1;
		if(mstruct1.isBitwiseNot()) return 1;
		if(mstruct2.isUndefined()) return -1;
		if(mstruct1.isUndefined()) return 1;
		if(mstruct2.isFunction()) return -1;
		if(mstruct1.isFunction()) return 1;
		if(mstruct2.isAddition()) return -1;
		if(mstruct1.isAddition()) return 1;
		if(mstruct2.isMultiplication()) return -1;
		if(mstruct1.isMultiplication()) return 1;
		if(mstruct2.isPower()) return -1;
		if(mstruct1.isPower()) return 1;
		if(mstruct2.isUnit()) return -1;
		if(mstruct1.isUnit()) return 1;
		if(mstruct2.isSymbolic()) return -1;
		if(mstruct1.isSymbolic()) return 1;
		if(mstruct2.isVariable()) return -1;
		if(mstruct1.isVariable()) return 1;
		if(parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		return -1;
	}
	switch(mstruct1.type()) {
		case STRUCT_NUMBER: {
			if(CALCULATOR->aborted()) return 0;
			if(b_abs) {
				ComparisonResult cmp = mstruct1.number().compareAbsolute(mstruct2.number());
				if(cmp == COMPARISON_RESULT_LESS) return -1;
				else if(cmp == COMPARISON_RESULT_GREATER) return 1;
				return 0;
			}
			if(!mstruct1.number().hasImaginaryPart() && !mstruct2.number().hasImaginaryPart()) {
				if(mstruct1.number().isFloatingPoint()) {
					if(!mstruct2.number().isFloatingPoint()) return 1;
					if(mstruct1.number().isInterval()) {
						if(!mstruct2.number().isInterval()) return 1;
					} else if(mstruct2.number().isInterval()) return -1;
				} else if(mstruct2.number().isFloatingPoint()) return -1;
				ComparisonResult cmp = mstruct1.number().compare(mstruct2.number());
				if(cmp == COMPARISON_RESULT_LESS) return -1;
				else if(cmp == COMPARISON_RESULT_GREATER) return 1;
				return 0;
			} else {
				if(!mstruct1.number().hasRealPart()) {
					if(mstruct2.number().hasRealPart()) {
						return 1;
					} else {
						ComparisonResult cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
						if(cmp == COMPARISON_RESULT_LESS) return -1;
						else if(cmp == COMPARISON_RESULT_GREATER) return 1;
						return 0;
					}
				} else if(mstruct2.number().hasRealPart()) {
					ComparisonResult cmp = mstruct1.number().compareRealParts(mstruct2.number());
					if(cmp == COMPARISON_RESULT_EQUAL) {
						cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
					} 
					if(cmp == COMPARISON_RESULT_LESS) return -1;
					else if(cmp == COMPARISON_RESULT_GREATER) return 1;
					return 0;
				} else {
					return -1;
				}
			}
			return -1;
		} 
		case STRUCT_UNIT: {
			if(mstruct1.unit() < mstruct2.unit()) return -1;
			if(mstruct1.unit() == mstruct2.unit()) return 0;
			return 1;
		}
		case STRUCT_SYMBOLIC: {
			if(mstruct1.symbol() < mstruct2.symbol()) return -1;
			else if(mstruct1.symbol() == mstruct2.symbol()) return 0;
			return 1;
		}
		case STRUCT_VARIABLE: {
			if(mstruct1.variable() < mstruct2.variable()) return -1;
			else if(mstruct1.variable() == mstruct2.variable()) return 0;
			return 1;
		}
		case STRUCT_FUNCTION: {
			if(mstruct1.function() < mstruct2.function()) return -1;
			if(mstruct1.function() == mstruct2.function()) {
				for(size_t i = 0; i < mstruct2.size(); i++) {
					if(i >= mstruct1.size()) {
						return -1;	
					}
					int i2 = evalSortCompare(mstruct1[i], mstruct2[i], parent, b_abs);
					if(i2 != 0) return i2;
				}
				return 0;
			}
			return 1;
		}
		case STRUCT_POWER: {
			int i = evalSortCompare(mstruct1[0], mstruct2[0], parent, b_abs);
			if(i == 0) {
				return evalSortCompare(mstruct1[1], mstruct2[1], parent, b_abs);
			}
			return i;
		}
		default: {
			if(mstruct2.size() < mstruct1.size()) return -1;
			else if(mstruct2.size() > mstruct1.size()) return 1;
			int ie;
			for(size_t i = 0; i < mstruct1.size(); i++) {
				ie = evalSortCompare(mstruct1[i], mstruct2[i], parent, b_abs);
				if(ie != 0) {
					return ie;
				}
			}
		}
	}
	return 0;
}

void MathStructure::evalSort(bool recursive, bool b_abs) {
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).evalSort(true, b_abs);
		}
	}
	//if(m_type != STRUCT_ADDITION && m_type != STRUCT_MULTIPLICATION && m_type != STRUCT_LOGICAL_AND && m_type != STRUCT_LOGICAL_OR && m_type != STRUCT_LOGICAL_XOR && m_type != STRUCT_BITWISE_AND && m_type != STRUCT_BITWISE_OR && m_type != STRUCT_BITWISE_XOR) return;
	if(m_type != STRUCT_ADDITION && m_type != STRUCT_MULTIPLICATION && m_type != STRUCT_BITWISE_AND && m_type != STRUCT_BITWISE_OR && m_type != STRUCT_BITWISE_XOR) return;
	vector<size_t> sorted;
	sorted.reserve(SIZE);
	for(size_t i = 0; i < SIZE; i++) {
		if(i == 0) {
			sorted.push_back(v_order[i]);
		} else {
			if(evalSortCompare(CHILD(i), *v_subs[sorted.back()], *this, b_abs) >= 0) {
				sorted.push_back(v_order[i]);
			} else if(sorted.size() == 1) {
				sorted.insert(sorted.begin(), v_order[i]);
			} else {
				for(size_t i2 = sorted.size() - 2; ; i2--) {
					if(evalSortCompare(CHILD(i), *v_subs[sorted[i2]], *this, b_abs) >= 0) {	
						sorted.insert(sorted.begin() + i2 + 1, v_order[i]);
						break;
					}
					if(i2 == 0) {
						sorted.insert(sorted.begin(), v_order[i]);
						break;
					}
				}
			}
		}
	}
	for(size_t i2 = 0; i2 < sorted.size(); i2++) {
		v_order[i2] = sorted[i2];
	}
}

int sortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, const PrintOptions &po);
int sortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, const PrintOptions &po) {
	if(parent.isMultiplication()) {
		if(!mstruct1.representsNonMatrix() && !mstruct2.representsNonMatrix()) {
			return 0;
		}
	}
	if(parent.isAddition() && po.sort_options.minus_last) {
		bool m1 = mstruct1.hasNegativeSign(), m2 = mstruct2.hasNegativeSign();
		if(m1 && !m2) {
			return 1;
		} else if(m2 && !m1) {
			return -1;
		}
	}
	if(parent.isAddition() && (mstruct1.isUnit() || (mstruct1.isMultiplication() && mstruct1.size() == 2 && mstruct1[1].isUnit())) && (mstruct2.isUnit() || (mstruct2.isMultiplication() && mstruct2.size() == 2 && mstruct2[1].isUnit()))) {
		Unit *u1, *u2;
		if(mstruct1.isUnit()) u1 = mstruct1.unit();
		else u1 = mstruct1[1].unit();
		if(mstruct2.isUnit()) u2 = mstruct2.unit();
		else u2 = mstruct2[1].unit();
		if(u1->isParentOf(u2)) return 1;
		if(u2->isParentOf(u1)) return -1;
	}
	bool isdiv1 = false, isdiv2 = false;
	if(!po.negative_exponents) {
		if(mstruct1.isMultiplication()) {
			for(size_t i = 0; i < mstruct1.size(); i++) {
				if(mstruct1[i].isPower() && mstruct1[i][1].hasNegativeSign()) {
					isdiv1 = true;
					break;
				}
			}
		} else if(mstruct1.isPower() && mstruct1[1].hasNegativeSign()) {
			isdiv1 = true;
		}
		if(mstruct2.isMultiplication()) {
			for(size_t i = 0; i < mstruct2.size(); i++) {
				if(mstruct2[i].isPower() && mstruct2[i][1].hasNegativeSign()) {
					isdiv2 = true;
					break;
				}
			}
		} else if(mstruct2.isPower() && mstruct2[1].hasNegativeSign()) {
			isdiv2 = true;
		}
	}
	if(parent.isAddition() && isdiv1 == isdiv2) {
		if(mstruct1.isMultiplication() && mstruct1.size() > 0) {
			size_t start = 0;
			while(mstruct1[start].isNumber() && mstruct1.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct2.isMultiplication()) {
				if(mstruct2.size() < 1) return -1;
				size_t start2 = 0;
				while(mstruct2[start2].isNumber() && mstruct2.size() > start2 + 1) {
					start2++;
				}
				for(size_t i = 0; i + start < mstruct1.size(); i++) {
					if(i + start2 >= mstruct2.size()) return 1;
					i2 = sortCompare(mstruct1[i + start], mstruct2[i + start2], parent, po);
					if(i2 != 0) return i2;
				}
				if(mstruct1.size() - start == mstruct2.size() - start2) return 0;
				if(parent.isMultiplication()) return -1;
				else return 1;
			} else {
				i2 = sortCompare(mstruct1[start], mstruct2, parent, po);
				if(i2 != 0) return i2;
			}
		} else if(mstruct2.isMultiplication() && mstruct2.size() > 0) {
			size_t start = 0;
			while(mstruct2[start].isNumber() && mstruct2.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct1.isMultiplication()) {
				return 1;
			} else {
				i2 = sortCompare(mstruct1, mstruct2[start], parent, po);
				if(i2 != 0) return i2;
			}
		} 
	}
	if(mstruct1.isSymbolic() && mstruct1.symbol() == "C") return 1;
	if(mstruct2.isSymbolic() && mstruct2.symbol() == "C") return -1;
	if(mstruct1.type() != mstruct2.type()) {
		if(mstruct1.isVariable() && mstruct2.isSymbolic()) {
			if(parent.isMultiplication()) {
				if(mstruct1.variable()->isKnown()) return -1;
			}
			if(mstruct1.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name < mstruct2.symbol()) return -1;
			else return 1;
		}
		if(mstruct2.isVariable() && mstruct1.isSymbolic()) {
			if(parent.isMultiplication()) {
				if(mstruct2.variable()->isKnown()) return 1;
			}
			if(mstruct1.symbol() < mstruct2.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			else return 1;
		}
		if(!parent.isMultiplication() || (!mstruct1.isNumber() && !mstruct2.isNumber())) {
			if(mstruct2.isPower()) {
				int i = sortCompare(mstruct1, mstruct2[0], parent, po);
				if(i == 0) {
					return sortCompare(m_one, mstruct2[1], parent, po);
				}
				return i;
			}
			if(mstruct1.isPower()) {
				int i = sortCompare(mstruct1[0], mstruct2, parent, po);
				if(i == 0) {
					return sortCompare(mstruct1[1], m_one, parent, po);
				}
				return i;
			}
		}
		if(parent.isMultiplication()) {
			if(mstruct2.isUnit()) return -1;
			if(mstruct1.isUnit()) return 1;
			if(mstruct1.isAddition() && !mstruct2.isAddition() && !mstruct1.containsUnknowns() && (mstruct2.isUnknown_exp() || (mstruct2.isMultiplication() && mstruct2.containsUnknowns()))) return -1;
			if(mstruct2.isAddition() && !mstruct1.isAddition() && !mstruct2.containsUnknowns() && (mstruct1.isUnknown_exp() || (mstruct1.isMultiplication() && mstruct1.containsUnknowns()))) return 1;
		}
		if(mstruct2.isAborted()) return -1;
		if(mstruct1.isAborted()) return 1;
		if(mstruct2.isInverse()) return -1;
		if(mstruct1.isInverse()) return 1;
		if(mstruct2.isDivision()) return -1;
		if(mstruct1.isDivision()) return 1;
		if(mstruct2.isNegate()) return -1;
		if(mstruct1.isNegate()) return 1;
		if(mstruct2.isLogicalAnd()) return -1;
		if(mstruct1.isLogicalAnd()) return 1;
		if(mstruct2.isLogicalOr()) return -1;
		if(mstruct1.isLogicalOr()) return 1;
		if(mstruct2.isLogicalXor()) return -1;
		if(mstruct1.isLogicalXor()) return 1;
		if(mstruct2.isLogicalNot()) return -1;
		if(mstruct1.isLogicalNot()) return 1;
		if(mstruct2.isComparison()) return -1;
		if(mstruct1.isComparison()) return 1;
		if(mstruct2.isBitwiseOr()) return -1;
		if(mstruct1.isBitwiseOr()) return 1;
		if(mstruct2.isBitwiseXor()) return -1;
		if(mstruct1.isBitwiseXor()) return 1;
		if(mstruct2.isBitwiseAnd()) return -1;
		if(mstruct1.isBitwiseAnd()) return 1;
		if(mstruct2.isBitwiseNot()) return -1;
		if(mstruct1.isBitwiseNot()) return 1;
		if(mstruct2.isUndefined()) return -1;
		if(mstruct1.isUndefined()) return 1;
		if(mstruct2.isFunction()) return -1;
		if(mstruct1.isFunction()) return 1;
		if(mstruct2.isAddition()) return -1;
		if(mstruct1.isAddition()) return 1;
		if(!parent.isMultiplication()) {
			if(isdiv2 && mstruct2.isMultiplication()) return -1;
			if(isdiv1 && mstruct1.isMultiplication()) return 1;
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		if(mstruct2.isMultiplication()) return -1;
		if(mstruct1.isMultiplication()) return 1;
		if(mstruct2.isPower()) return -1;
		if(mstruct1.isPower()) return 1;
		if(mstruct2.isUnit()) return -1;
		if(mstruct1.isUnit()) return 1;
		if(mstruct2.isSymbolic()) return -1;
		if(mstruct1.isSymbolic()) return 1;
		if(mstruct2.isVariable()) return -1;
		if(mstruct1.isVariable()) return 1;
		if(parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		return -1;
	}
	switch(mstruct1.type()) {
		case STRUCT_NUMBER: {
			if(!mstruct1.number().hasImaginaryPart() && !mstruct2.number().hasImaginaryPart()) {
				ComparisonResult cmp;
				if(parent.isMultiplication() && mstruct2.number().isNegative() != mstruct1.number().isNegative()) cmp = mstruct2.number().compare(mstruct1.number());
				else cmp = mstruct1.number().compare(mstruct2.number());
				if(cmp == COMPARISON_RESULT_LESS) return -1;
				else if(cmp == COMPARISON_RESULT_GREATER) return 1;
				return 0;
			} else {
				if(!mstruct1.number().hasRealPart()) {
					if(mstruct2.number().hasRealPart()) {
						return 1;
					} else {
						ComparisonResult cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
						if(cmp == COMPARISON_RESULT_LESS) return -1;
						else if(cmp == COMPARISON_RESULT_GREATER) return 1;
						return 0;
					}
				} else if(mstruct2.number().hasRealPart()) {
					ComparisonResult cmp = mstruct1.number().compareRealParts(mstruct2.number());
					if(cmp == COMPARISON_RESULT_EQUAL) {
						cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
					} 
					if(cmp == COMPARISON_RESULT_LESS) return -1;
					else if(cmp == COMPARISON_RESULT_GREATER) return 1;
					return 0;
				} else {
					return -1;
				}
			}
			return -1;
		} 
		case STRUCT_UNIT: {
			if(mstruct1.unit() == mstruct2.unit()) return 0;
			if(mstruct1.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct1.isPlural(), po.use_reference_names).name < mstruct2.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct2.isPlural(), po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_SYMBOLIC: {
			if(mstruct1.symbol() < mstruct2.symbol()) return -1;
			else if(mstruct1.symbol() == mstruct2.symbol()) return 0;
			return 1;
		}
		case STRUCT_VARIABLE: {
			if(mstruct1.variable() == mstruct2.variable()) return 0;
			if(parent.isMultiplication()) {
				if(mstruct1.variable()->isKnown() && !mstruct2.variable()->isKnown()) return -1;
				if(!mstruct1.variable()->isKnown() && mstruct2.variable()->isKnown()) return 1;
			}
			if(mstruct1.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names).name < mstruct2.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_FUNCTION: {
			if(mstruct1.function() == mstruct2.function()) {
				for(size_t i = 0; i < mstruct2.size(); i++) {
					if(i >= mstruct1.size()) {
						return -1;	
					}
					int i2 = sortCompare(mstruct1[i], mstruct2[i], parent, po);
					if(i2 != 0) return i2;
				}
				return 0;
			}
			if(mstruct1.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names).name < mstruct2.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_POWER: {
			if(!po.negative_exponents) {
				bool b1 = mstruct1[1].hasNegativeSign();
				bool b2 = mstruct2[1].hasNegativeSign();
				if(b1 && !b2) return -1;
				if(b2 && !b1) return 1;
			}
			int i = sortCompare(mstruct1[0], mstruct2[0], parent, po);
			if(i == 0) {
				return sortCompare(mstruct1[1], mstruct2[1], parent, po);
			}
			return i;
		}
		case STRUCT_MULTIPLICATION: {
			if(isdiv1 != isdiv2) {
				if(isdiv1) return 1;
				return -1;
			}
		}
		case STRUCT_COMPARISON: {
			if((mstruct1.comparisonType() == COMPARISON_LESS || mstruct1.comparisonType() == COMPARISON_EQUALS_LESS) && (mstruct2.comparisonType() == COMPARISON_GREATER || mstruct2.comparisonType() == COMPARISON_EQUALS_GREATER)) {
				return 1;
			}
			if((mstruct1.comparisonType() == COMPARISON_GREATER || mstruct1.comparisonType() == COMPARISON_EQUALS_GREATER) && (mstruct2.comparisonType() == COMPARISON_LESS || mstruct2.comparisonType() == COMPARISON_EQUALS_LESS)) {
				return -1;
			}
		}
		default: {
			int ie;
			for(size_t i = 0; i < mstruct1.size(); i++) {
				if(i >= mstruct2.size()) return 1;
				ie = sortCompare(mstruct1[i], mstruct2[i], parent, po);
				if(ie != 0) {
					return ie;
				}
			}
		}
	}
	return 0;
}

void MathStructure::sort(const PrintOptions &po, bool recursive) {
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CALCULATOR->aborted()) break;
			CHILD(i).sort(po);
		}
	}
	if(m_type != STRUCT_ADDITION && m_type != STRUCT_MULTIPLICATION && m_type != STRUCT_BITWISE_AND && m_type != STRUCT_BITWISE_OR && m_type != STRUCT_BITWISE_XOR && m_type != STRUCT_LOGICAL_AND && m_type != STRUCT_LOGICAL_OR) return;
	vector<size_t> sorted;
	bool b;
	PrintOptions po2 = po;
	po2.sort_options.minus_last = po.sort_options.minus_last && SIZE == 2;
	//!containsUnknowns();
	for(size_t i = 0; i < SIZE; i++) {
		if(CALCULATOR->aborted()) break;
		b = false;
		for(size_t i2 = 0; i2 < sorted.size(); i2++) {
			if(sortCompare(CHILD(i), *v_subs[sorted[i2]], *this, po2) < 0) {
				sorted.insert(sorted.begin() + i2, v_order[i]);
				b = true;
				break;
			}
		}
		if(!b) sorted.push_back(v_order[i]);
	}
	if(m_type == STRUCT_ADDITION && SIZE > 2 && po.sort_options.minus_last && v_subs[sorted[0]]->hasNegativeSign()) {
		for(size_t i2 = 1; i2 < sorted.size(); i2++) {
			if(CALCULATOR->aborted()) break;
			if(!v_subs[sorted[i2]]->hasNegativeSign()) {
				sorted.insert(sorted.begin(), sorted[i2]);
				sorted.erase(sorted.begin() + (i2 + 1));
				break;
			}
		}
	}
	for(size_t i2 = 0; i2 < sorted.size(); i2++) {
		v_order[i2] = sorted[i2];
	}
}
bool MathStructure::containsOpaqueContents() const {
	if(isFunction()) return true;
	if(isUnit() && o_unit->subtype() != SUBTYPE_BASE_UNIT) return true;
	if(isVariable() && o_variable->isKnown()) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsOpaqueContents()) return true;
	}
	return false;
}
bool MathStructure::containsAdditionPower() const {
	if(m_type == STRUCT_POWER && CHILD(0).isAddition()) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsAdditionPower()) return true;
	}
	return false;
}

size_t MathStructure::countTotalChildren(bool count_function_as_one) const {
	if((m_type == STRUCT_FUNCTION && count_function_as_one) || SIZE == 0) return 1;
	size_t count = 0;
	for(size_t i = 0; i < SIZE; i++) {
		count += CHILD(i).countTotalChildren() + 1;
	}
	return count;
}
int test_comparisons(const MathStructure &msave, MathStructure &mthis, const MathStructure &x_var, const EvaluationOptions &eo, bool sub = false);
bool try_isolate_x(MathStructure &mstruct, EvaluationOptions &eo3, const EvaluationOptions &eo);
bool try_isolate_x(MathStructure &mstruct, EvaluationOptions &eo3, const EvaluationOptions &eo) {
	if(mstruct.isProtected()) return false;
	if(mstruct.isComparison()) {
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mtest(mstruct);
		eo3.test_comparisons = false;
		eo3.warn_about_denominators_assumed_nonzero = false;
		mtest[0].calculatesub(eo3, eo);
		mtest[1].calculatesub(eo3, eo);
		eo3.test_comparisons = eo.test_comparisons;
		const MathStructure *x_var2;
		if(eo.isolate_var) x_var2 = eo.isolate_var;
		else x_var2 = &mstruct.find_x_var();
		if(x_var2->isUndefined() || (mtest[0] == *x_var2 && !mtest[1].contains(*x_var2))) {
			CALCULATOR->endTemporaryStopMessages();
			 return false;
		}
		if(mtest.isolate_x(eo3, eo, *x_var2, false)) {
			if(test_comparisons(mstruct, mtest, *x_var2, eo3) >= 0) {
				CALCULATOR->endTemporaryStopMessages(true);
				mstruct = mtest;
				return true;
			}
		}
		CALCULATOR->endTemporaryStopMessages();
	} else {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(try_isolate_x(mstruct[i], eo3, eo)) b = true;
		}
		return b;
	}
	return false;
}

bool compare_delete(MathStructure &mnum, MathStructure &mden, bool &erase1, bool &erase2, const EvaluationOptions &eo) {
	erase1 = false;
	erase2 = false;
	if(mnum == mden) {
		if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mnum.representsZero(true)) 
		|| mnum.representsNonZero(true)
		|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mnum.representsZero(true) && warn_about_denominators_assumed_nonzero(mnum, eo))) {
			erase1 = true;
			erase2 = true;			
		} else {
			if(mnum.isPower()) {
				mnum.setToChild(1);
				mden.setToChild(1);
				return true;
			}
			return false;
		}
		return true;
	}
	if(!mnum.isPower() && !mden.isPower()) return false;
	MathStructure *mbase1, *mbase2, *mexp1 = NULL, *mexp2 = NULL;
	if(mnum.isPower()) {
		if(!IS_REAL(mnum[1])) return false;
		mexp1 = &mnum[1];
		mbase1 = &mnum[0];
	} else {
		mbase1 = &mnum;
	}
	if(mden.isPower()) {
		if(!IS_REAL(mden[1])) return false;
		mexp2 = &mden[1];
		mbase2 = &mden[0];
	} else {
		mbase2 = &mden;
	}
	if(mbase1->equals(*mbase2)) {
		if(mexp1 && mexp2) {
			if(mexp1->number().isLessThan(mexp2->number())) {
				erase1 = true;
				mexp2->number() -= mexp1->number();
				if(mexp2->isOne()) mden.setToChild(1, true);
			} else {
				if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true))
				|| mbase2->representsNonZero(true)
				|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true) && warn_about_denominators_assumed_nonzero(*mbase2, eo))) {
					erase2 = true;
					mexp1->number() -= mexp2->number();
					if(mexp1->isOne()) mnum.setToChild(1, true);
				} else {
					if(mexp2->number().isFraction()) return false;
					mexp2->number()--;
					mexp1->number() -= mexp2->number();
					if(mexp1->isOne()) mnum.setToChild(1, true);
					if(mexp2->isOne()) mden.setToChild(1, true);
					return true;
				}
				
			}
			return true;	
		} else if(mexp1) {
			if(mexp1->number().isFraction()) {
				erase1 = true;
				mbase2->raise(m_one);
				(*mbase2)[1].number() -= mexp1->number();
				return true;
			}
			if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true))
			|| mbase2->representsNonZero(true)
			|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true) && warn_about_denominators_assumed_nonzero(*mbase2, eo))) {
				mexp1->number()--;
				erase2 = true;
				if(mexp1->isOne()) mnum.setToChild(1, true);
				return true;
			}
			return false;
			
		} else if(mexp2) {
			if(mexp2->number().isFraction()) {
				if((!eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true))
				|| mbase2->representsNonZero(true)
				|| (eo.warn_about_denominators_assumed_nonzero && eo.assume_denominators_nonzero && !mbase2->representsZero(true) && warn_about_denominators_assumed_nonzero(*mbase2, eo))) {
					erase2 = true;
					mbase1->raise(m_one);
					(*mbase1)[1].number() -= mexp2->number();
					return true;
				}
				return false;
			}
			mexp2->number()--;
			erase1 = true;
			if(mexp2->isOne()) mden.setToChild(1, true);
			return true;
		}
	}
	return false;
}

bool factor1(const MathStructure &mstruct, MathStructure &mnum, MathStructure &mden, const EvaluationOptions &eo) {
	mnum.setUndefined();
	mden.setUndefined();
	if(mstruct.isAddition()) {
		bool b_num = false, b_den = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isMultiplication()) {
				for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
					if(mstruct[i][i2].isPower() && mstruct[i][i2][1].hasNegativeSign()) {
						b_den = true;
						if(b_num) break;
					} else {
						b_num = true;
						if(b_den) break;
					}
				}
			} else if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
				b_den = true;
				if(b_num) break;
			} else {
				b_num = true;
				if(b_den) break;
			}
		}
		if(b_num && b_den) {
			MathStructure *mden_cur = NULL;
			vector<int> multi_index;
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(mnum.isUndefined()) {
					mnum.transform(STRUCT_ADDITION);
				} else {
					mnum.addChild(m_undefined);
				}
				if(mstruct[i].isMultiplication()) {
					if(!mden_cur) {
						mden_cur = new MathStructure();
						mden_cur->setUndefined();
					}
					for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
						if(mstruct[i][i2].isPower() && mstruct[i][i2][1].hasNegativeSign()) {
							if(mden_cur->isUndefined()) {
								if(mstruct[i][i2][1].isMinusOne()) {
									*mden_cur = mstruct[i][i2][0];
								} else if(mstruct[i][i2][1].isNumber()) {
									*mden_cur = mstruct[i][i2];
									(*mden_cur)[1].number().negate();
								} else {
									*mden_cur = mstruct[i][i2];
									(*mden_cur)[1][0].number().negate();
								}
							} else {
								if(mstruct[i][i2][1].isMinusOne()) {
									mden_cur->multiply(mstruct[i][i2][0], true);
								} else if(mstruct[i][i2][1].isNumber()) {
									mden_cur->multiply(mstruct[i][i2], true);
									(*mden_cur)[mden_cur->size() - 1][1].number().negate();
								} else {
									mden_cur->multiply(mstruct[i][i2], true);
									(*mden_cur)[mden_cur->size() - 1][1][0].number().negate();
								}
							}
						} else {
							if(mnum[mnum.size() - 1].isUndefined()) {
								mnum[mnum.size() - 1] = mstruct[i][i2];
							} else {
								mnum[mnum.size() - 1].multiply(mstruct[i][i2], true);
							}
						}
					}
					if(mnum[mnum.size() - 1].isUndefined()) mnum[mnum.size() - 1].set(1, 1, 0);
					if(mden_cur->isUndefined()) {
						multi_index.push_back(-1);
					} else {
						multi_index.push_back(mden.size());
						if(mden.isUndefined()) {
							mden.transform(STRUCT_MULTIPLICATION);
							mden[mden.size() - 1].set_nocopy(*mden_cur);
							mden_cur->unref();
						} else {
							mden.addChild_nocopy(mden_cur);
						}
						mden_cur = NULL;
					}
				} else if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
					multi_index.push_back(mden.size());
					if(mden.isUndefined()) {
						mden.transform(STRUCT_MULTIPLICATION);
					} else {
						mden.addChild(m_undefined);
					}
					if(mstruct[i][1].isMinusOne()) {
						mden[mden.size() - 1] = mstruct[i][0];
					} else {
						mden[mden.size() - 1] = mstruct[i];
						unnegate_sign(mden[mden.size() - 1][1]);
					}
					mnum[mnum.size() - 1].set(1, 1, 0);
				} else {
					multi_index.push_back(-1);
					mnum[mnum.size() - 1] = mstruct[i];
				}
			}			
			for(size_t i = 0; i < mnum.size(); i++) {
				if(multi_index[i] < 0 && mnum[i].isOne()) {
					if(mden.size() == 1) {
						mnum[i] = mden[0];
					} else {
						mnum[i] = mden;
					}
				} else {
					for(size_t i2 = 0; i2 < mden.size(); i2++) {
						if((long int) i2 != multi_index[i]) {
							mnum[i].multiply(mden[i2], true);
						}
					}					
				}
				mnum[i].calculatesub(eo, eo, false);
			}
			if(mden.size() == 1) {
				mden.setToChild(1);
			} else {
				mden.calculatesub(eo, eo, false);
			}	
			mnum.calculatesub(eo, eo, false);
		}
	} else if(mstruct.isMultiplication()) {
		bool b_num = false, b_den = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
				b_den = true;
				if(b_num) break;
			} else {
				b_num = true;
				if(b_den) break;
			}
		}
		if(b_den && b_num) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
					if(mden.isUndefined()) {
						if(mstruct[i][1].isMinusOne()) {
							mden = mstruct[i][0];
						} else {
							mden = mstruct[i];
							unnegate_sign(mden[1]);
						}
					} else {
						if(mstruct[i][1].isMinusOne()) {
							mden.multiply(mstruct[i][0], true);
						} else {
							mden.multiply(mstruct[i], true);
							unnegate_sign(mden[mden.size() - 1][1]);
						}
					}
				} else {
					if(mnum.isUndefined()) {
						mnum = mstruct[i];
					} else {
						mnum.multiply(mstruct[i], true);
					}
				}
			}
		}
		mden.calculatesub(eo, eo, false);
	}
	if(!mnum.isUndefined() && !mden.isUndefined()) {
		while(true) {
			mnum.factorize(eo, false, false, 0, true);
			mnum.evalSort(true);
			if(mnum.isMultiplication()) {
				for(size_t i = 0; i < mnum.size(); ) {
					if(mnum[i].isPower() && mnum[i][1].hasNegativeSign()) {
						if(mnum[i][1].isMinusOne()) {
							mnum[i][0].ref();
							mden.multiply_nocopy(&mnum[i][0], true);
						} else {
							mnum[i].ref();
							mden.multiply_nocopy(&mnum[i], true);
							unnegate_sign(mden[mden.size() - 1][1]);
						}
						mden.calculateMultiplyLast(eo);
						mnum.delChild(i + 1);
					} else {
						i++;
					}
				}
				if(mnum.size() == 0) mnum.set(1, 1, 0);
				else if(mnum.size() == 1) mnum.setToChild(1);
			}
			mden.factorize(eo, false, false, 0, true);
			mden.evalSort(true);
			bool b = false;
			if(mden.isMultiplication()) {
				for(size_t i = 0; i < mden.size(); ) {
					if(mden[i].isPower() && mden[i][1].hasNegativeSign()) {
						MathStructure *mden_inv = new MathStructure(mden[i]);
						if(mden_inv->calculateInverse(eo)) {
							mnum.multiply_nocopy(mden_inv, true);
							mnum.calculateMultiplyLast(eo);
							mden.delChild(i + 1);
							b = true;
						} else {
							mden_inv->unref();
							i++;
						}
					} else {
						i++;
					}
				}
				if(mden.size() == 0) mden.set(1, 1, 0);
				else if(mden.size() == 1) mden.setToChild(1);
			}
			if(!b) break;
		}
		bool erase1, erase2;
		if(mnum.isMultiplication()) {
			for(long int i = 0; i < (long int) mnum.size(); i++) {
				if(mden.isMultiplication()) {
					for(size_t i2 = 0; i2 < mden.size(); i2++) {
						if(compare_delete(mnum[i], mden[i2], erase1, erase2, eo)) {
							if(erase1) mnum.delChild(i + 1);
							if(erase2) mden.delChild(i2 + 1);
							i--;
							break;
						}
					}
				} else {
					if(compare_delete(mnum[i], mden, erase1, erase2, eo)) {
						if(erase1) mnum.delChild(i + 1);
						if(erase2) mden.set(1, 1, 0);
						break;
					}
				}
			}
		} else if(mden.isMultiplication()) {
			for(size_t i = 0; i < mden.size(); i++) {
				if(compare_delete(mnum, mden[i], erase1, erase2, eo)) {
					if(erase1) mnum.set(1, 1, 0);
					if(erase2) mden.delChild(i + 1);
					break;
				}
			}
		} else {
			if(compare_delete(mnum, mden, erase1, erase2, eo)) {
				if(erase1) mnum.set(1, 1, 0);
				if(erase2) mden.set(1, 1, 0);
			}
		}
		if(mnum.isMultiplication()) {
			if(mnum.size() == 0) mnum.set(1, 1, 0);
			else if(mnum.size() == 1) mnum.setToChild(1);
		}
		if(mden.isMultiplication()) {
			if(mden.size() == 0) mden.set(1, 1, 0);
			else if(mden.size() == 1) mden.setToChild(1);
		}
		return true;
	}
	return false;
}

bool combination_factorize(MathStructure &mstruct) {
	bool retval = false;
	switch(mstruct.type()) {
		case STRUCT_ADDITION: {
			bool b = false;
			// 5/y + x/y + z = (5 + x)/y + z
			MathStructure mstruct_units(mstruct);
			MathStructure mstruct_new(mstruct);
			for(size_t i = 0; i < mstruct_units.size(); i++) {
				if(mstruct_units[i].isMultiplication()) {
					for(size_t i2 = 0; i2 < mstruct_units[i].size();) {
						if(!mstruct_units[i][i2].isPower() || !mstruct_units[i][i2][1].hasNegativeSign()) {
							mstruct_units[i].delChild(i2 + 1);
						} else {
							i2++;
						}
					}
					if(mstruct_units[i].size() == 0) mstruct_units[i].setUndefined();
					else if(mstruct_units[i].size() == 1) mstruct_units[i].setToChild(1);
					for(size_t i2 = 0; i2 < mstruct_new[i].size();) {
						if(mstruct_new[i][i2].isPower() && mstruct_new[i][i2][1].hasNegativeSign()) {
							mstruct_new[i].delChild(i2 + 1);
						} else {
							i2++;
						}
					}
					if(mstruct_new[i].size() == 0) mstruct_new[i].set(1, 1, 0);
					else if(mstruct_new[i].size() == 1) mstruct_new[i].setToChild(1);
				} else if(mstruct_new[i].isPower() && mstruct_new[i][1].hasNegativeSign()) {
					mstruct_new[i].set(1, 1, 0);
				} else {
					mstruct_units[i].setUndefined();
				}
			}
			for(size_t i = 0; i < mstruct_units.size(); i++) {
				if(!mstruct_units[i].isUndefined()) {
					for(size_t i2 = i + 1; i2 < mstruct_units.size();) {
						if(mstruct_units[i2] == mstruct_units[i]) {
							mstruct_new[i].add(mstruct_new[i2], true);
							mstruct_new.delChild(i2 + 1);
							mstruct_units.delChild(i2 + 1);
							b = true;
						} else {
							i2++;
						}
					}
					if(mstruct_new[i].isOne()) mstruct_new[i].set(mstruct_units[i]);
					else mstruct_new[i].multiply(mstruct_units[i], true);
				}
			}
			if(b) {
				if(mstruct_new.size() == 1) {
					mstruct.set(mstruct_new[0], true);
				} else {
					mstruct = mstruct_new;
				}
				b = false;
				retval = true;
			}
			if(!b && mstruct.isAddition()) {
				// 5x + pi*x + 5y + xy = (5 + pi)x + 5y + xy
				MathStructure mstruct_units(mstruct);
				MathStructure mstruct_new(mstruct);
				for(size_t i = 0; i < mstruct_units.size(); i++) {
					if(mstruct_units[i].isMultiplication()) {
						for(size_t i2 = 0; i2 < mstruct_units[i].size();) {
							if(!mstruct_units[i][i2].containsType(STRUCT_UNIT, true) && !mstruct_units[i][i2].containsUnknowns()) {
								mstruct_units[i].delChild(i2 + 1);
							} else {
								i2++;
							}
						}
						if(mstruct_units[i].size() == 0) mstruct_units[i].setUndefined();
						else if(mstruct_units[i].size() == 1) mstruct_units[i].setToChild(1);
						for(size_t i2 = 0; i2 < mstruct_new[i].size();) {
							if(mstruct_new[i][i2].containsType(STRUCT_UNIT, true) || mstruct_new[i][i2].containsUnknowns()) {
								mstruct_new[i].delChild(i2 + 1);
							} else {
								i2++;
							}
						}
						if(mstruct_new[i].size() == 0) mstruct_new[i].set(1, 1, 0);
						else if(mstruct_new[i].size() == 1) mstruct_new[i].setToChild(1);
					} else if(mstruct_units[i].containsType(STRUCT_UNIT, true) || mstruct_units[i].containsUnknowns()) {
						mstruct_new[i].set(1, 1, 0);
					} else {
						mstruct_units[i].setUndefined();
					}
				}
				for(size_t i = 0; i < mstruct_units.size(); i++) {
					if(!mstruct_units[i].isUndefined()) {
						for(size_t i2 = i + 1; i2 < mstruct_units.size();) {
							if(mstruct_units[i2] == mstruct_units[i]) {
								mstruct_new[i].add(mstruct_new[i2], true);
								mstruct_new.delChild(i2 + 1);
								mstruct_units.delChild(i2 + 1);
								b = true;
							} else {
								i2++;
							}
						}
						if(mstruct_new[i].isOne()) mstruct_new[i].set(mstruct_units[i]);
						else mstruct_new[i].multiply(mstruct_units[i], true);
					}
				}
				if(b) {
					if(mstruct_new.size() == 1) mstruct.set(mstruct_new[0], true);
					else mstruct = mstruct_new;
					retval = true;
				}
			}
			//if(retval) return retval;
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(combination_factorize(mstruct[i])) {
					mstruct.childUpdated(i);
					b = true;
				}
			}
			if(b) retval = true;
		}
	}
	return retval;
}

bool MathStructure::simplify(const EvaluationOptions &eo, bool unfactorize) {

	if(SIZE == 0) return false;

	if(unfactorize) {
		unformat();
		EvaluationOptions eo2 = eo;
		eo2.expand = true;
		eo2.combine_divisions = false;
		eo2.sync_units = false;
		calculatesub(eo2, eo2);
		bool b = do_simplification(*this, eo2, true, false, false);
		return combination_factorize(*this) || b;
	}
	return combination_factorize(*this);

}

bool MathStructure::structure(StructuringMode structuring, const EvaluationOptions &eo, bool restore_first) {
	switch(structuring) {
		case STRUCTURING_NONE: {
			if(restore_first) {
				EvaluationOptions eo2 = eo;
				eo2.sync_units = false;
				calculatesub(eo2, eo2);
			}
			return false;
		}
		case STRUCTURING_FACTORIZE: {
			return factorize(eo, restore_first);
		}
		default: {
			return simplify(eo, restore_first);
		}
	}
}

void clean_multiplications(MathStructure &mstruct);
void clean_multiplications(MathStructure &mstruct) {
	if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isMultiplication()) {
				size_t i2 = 0;
				for(; i2 < mstruct[i + i2].size(); i2++) {
					mstruct[i + i2][i2].ref();
					mstruct.insertChild_nocopy(&mstruct[i + i2][i2], i + i2 + 1);
				}
				mstruct.delChild(i + i2 + 1);
			}
		}
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		clean_multiplications(mstruct[i]);
	}
}

bool containsComplexUnits(const MathStructure &mstruct) {
	if(mstruct.type() == STRUCT_UNIT && mstruct.unit()->hasComplexRelationTo(mstruct.unit()->baseUnit())) return true;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(containsComplexUnits(mstruct[i])) {
			return true;
		}
	}
	return false;
}

bool variablesContainsComplexUnits(const MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_VARIABLE && mstruct.variable()->isKnown() && (eo.approximation != APPROXIMATION_EXACT || !mstruct.variable()->isApproximate()) && ((KnownVariable*) mstruct.variable())->get().containsType(STRUCT_UNIT, false, true, true)) {
		return containsComplexUnits(((KnownVariable*) mstruct.variable())->get());
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(variablesContainsComplexUnits(mstruct[i], eo)) {
			return true;
		}
	}
	return false;
}


bool expandVariablesWithUnits(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_VARIABLE && mstruct.variable()->isKnown() && (eo.approximation != APPROXIMATION_EXACT || !mstruct.variable()->isApproximate()) && ((KnownVariable*) mstruct.variable())->get().containsType(STRUCT_UNIT, false, true, true) != 0) {
		mstruct.set(((KnownVariable*) mstruct.variable())->get());
		return true;
	}
	bool retval = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(expandVariablesWithUnits(mstruct[i], eo)) {
			mstruct.childUpdated(i + 1);
			retval = true;
		}
	}
	return retval;
}

#define IS_VAR_EXP(x) ((x.isVariable() && x.variable()->isKnown()) || (x.isPower() && x[0].isVariable() && x[0].variable()->isKnown()))

void factorize_variable(MathStructure &mstruct, const MathStructure &mvar, bool deg2) {
	if(deg2) {
		// ax^2+bx = (sqrt(b)*x+(a/sqrt(b))/2)^2-((a/sqrt(b))/2)^2
		MathStructure a_struct, b_struct, mul_struct(1, 1, 0);
		for(size_t i2 = 0; i2 < mstruct.size();) {
			bool b = false;
			if(mstruct[i2] == mvar) {
				a_struct.set(1, 1, 0);
				b = true;
			} else if(mstruct[i2].isPower() && mstruct[i2][0] == mvar && mstruct[i2][1].isNumber() && mstruct[i2][1].number().isTwo()) {
				b_struct.set(1, 1, 0);
				b = true;
			} else if(mstruct[i2].isMultiplication()) {
				for(size_t i3 = 0; i3 < mstruct[i2].size(); i3++) {
					if(mstruct[i2][i3] == mvar) {
						a_struct = mstruct[i2];
						a_struct.delChild(i3 + 1);
						b = true;
						break;
					} else if(mstruct[i2][i3].isPower() && mstruct[i2][i3][0] == mvar && mstruct[i2][i3][1].isNumber() && mstruct[i2][i3][1].number().isTwo()) {
						b_struct = mstruct[i2];
						b_struct.delChild(i3 + 1);
						b = true;
						break;
					}
				}
			}
			if(b) {
				mstruct.delChild(i2 + 1);
			} else {
				i2++;
			}
		}
		if(b_struct == a_struct) {
			mul_struct = a_struct;
			a_struct.set(1, 1, 0);
			b_struct.set(1, 1, 0);
		} else if(b_struct.isMultiplication() && a_struct.isMultiplication()) {
			size_t i3 = 0;
			for(size_t i = 0; i < a_struct.size();) {
				bool b = false;
				for(size_t i2 = i3; i2 < b_struct.size(); i2++) {
					if(a_struct[i] == b_struct[i2]) {
						i3 = i2;
						if(mul_struct.isOne()) mul_struct = a_struct[i];
						else mul_struct.multiply(a_struct[i], true);
						a_struct.delChild(i + 1);
						b_struct.delChild(i2 + 1);
						b = true;
						break;
					}
				}
				if(!b) i++;
			}
		}
		if(a_struct.isMultiplication() && a_struct.size() == 0) b_struct.set(1, 1, 0);
		else if(a_struct.isMultiplication() && a_struct.size() == 1) b_struct.setToChild(1);
		if(b_struct.isMultiplication() && b_struct.size() == 0) b_struct.set(1, 1, 0);
		else if(b_struct.isMultiplication() && b_struct.size() == 1) b_struct.setToChild(1);
		if(!b_struct.isOne()) {
			b_struct.raise(nr_half);
			a_struct.divide(b_struct);
		}
		a_struct.multiply(nr_half);
		if(b_struct.isOne()) b_struct = mvar;
		else b_struct *= mvar;
		b_struct += a_struct;
		b_struct.raise(nr_two);
		a_struct.raise(nr_two);
		a_struct.negate();
		b_struct += a_struct;
		if(mstruct.size() == 0) mstruct = b_struct;
		else mstruct.addChild(b_struct);
	} else {
		vector<MathStructure*> left_structs;
		for(size_t i2 = 0; i2 < mstruct.size();) {
			bool b = false;
			if(mstruct[i2] == mvar) {
				mstruct[i2].set(1, 1, 0, true);
				b = true;
			} else if(mstruct[i2].isMultiplication()) {
				for(size_t i3 = 0; i3 < mstruct[i2].size(); i3++) {
					if(mstruct[i2][i3] == mvar) {
						mstruct[i2].delChild(i3 + 1, true);
						b = true;
						break;
					}
				}
			}
			if(b) {
				i2++;
			} else {
				mstruct[i2].ref();
				left_structs.push_back(&mstruct[i2]);
				mstruct.delChild(i2 + 1);
			}
		}
		mstruct.multiply(mvar);
		for(size_t i = 0; i < left_structs.size(); i++) {
			mstruct.add_nocopy(left_structs[i], true);
			mstruct.evalSort(false);
		}
	}
}

bool var_contains_interval(const MathStructure &mstruct) {
	if(mstruct.isNumber()) return mstruct.number().isInterval();
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_interval) return true;
	if(mstruct.isVariable() && mstruct.variable()->isKnown()) return var_contains_interval(((KnownVariable*) mstruct.variable())->get());
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(var_contains_interval(mstruct[i])) return true;
	}
	return false;
}

bool factorize_variables(MathStructure &mstruct, const EvaluationOptions &eo) {
	bool b = false;
	if(mstruct.type() == STRUCT_ADDITION) {
		vector<MathStructure> variables;
		vector<size_t> variable_count;
		vector<int> term_sgn;
		vector<bool> term_deg2;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(CALCULATOR->aborted()) break;
			if(mstruct[i].isMultiplication()) {
				for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
					if(CALCULATOR->aborted()) break;
					if(IS_VAR_EXP(mstruct[i][i2])) {
						bool b_found = false;
						for(size_t i3 = 0; i3 < variables.size(); i3++) {
							if(variables[i3] == mstruct[i][i2]) {
								variable_count[i3]++;
								b_found = true;
								if(term_sgn[i3] == 1 && !mstruct[i].representsNonNegative(true)) term_sgn[i3] = 0;
								else if(term_sgn[i3] == -1 && !mstruct[i].representsNonPositive(true)) term_sgn[i3] = 0;
								break;
							}
						}
						if(!b_found) {
							variables.push_back(mstruct[i][i2]);
							variable_count.push_back(1);
							term_deg2.push_back(false);
							if(mstruct[i].representsNonNegative(true)) term_sgn.push_back(1);
							else if(mstruct[i].representsNonPositive(true)) term_sgn.push_back(-1);
							else term_sgn.push_back(0);
						}
					}
				}
			} else if(IS_VAR_EXP(mstruct[i])) {
				bool b_found = false;
				for(size_t i3 = 0; i3 < variables.size(); i3++) {
					if(variables[i3] == mstruct[i]) {
						variable_count[i3]++;
						b_found = true;
						if(term_sgn[i3] == 1 && !mstruct[i].representsNonNegative(true)) term_sgn[i3] = 0;
						else if(term_sgn[i3] == -1 && !mstruct[i].representsNonPositive(true)) term_sgn[i3] = 0;
						break;
					}
				}
				if(!b_found) {
					variables.push_back(mstruct[i]);
					variable_count.push_back(1);
					term_deg2.push_back(false);
					if(mstruct[i].representsNonNegative(true)) term_sgn.push_back(1);
					else if(mstruct[i].representsNonPositive(true)) term_sgn.push_back(-1);
					else term_sgn.push_back(0);
				}
			}
		}
		for(size_t i = 0; i < variables.size();) {
			bool b_erase = false;
			if(!var_contains_interval(variables[i])) {
				b_erase = true;
			} else if(variable_count[i] == 1 || term_sgn[i] != 0) {
				b_erase = true;
				if(!variables[i].isPower() || (variables[i][1].isNumber() && variables[i][1].number().isTwo())) {
					for(size_t i2 = i + 1; i2 < variables.size(); i2++) {
						if((variables[i].isPower() && !variables[i2].isPower() && variables[i][0] == variables[i2])|| (!variables[i].isPower() && variables[i2].isPower() && variables[i2][0] == variables[i] && variables[i2][1].isNumber() && variables[i2][1].number().isTwo())) {
							bool b_erase2 = false;
							if(variable_count[i2] == 1) {
								if(term_sgn[i] == 0 || term_sgn[i2] != term_sgn[i]) {
									if(variable_count[i] == 1) {
										term_deg2[i] = true;
										variable_count[i] = 2;
										term_sgn[i] = 0;
										if(variables[i].isPower()) variables[i].setToChild(1);
									} else {
										term_sgn[i] = 0;
									}
									b_erase = false;
								}
								b_erase2 = true;
							} else if(term_sgn[i2] != 0) {
								if(term_sgn[i] == 0 || term_sgn[i2] != term_sgn[i]) {
									if(variable_count[i] != 1) {
										term_sgn[i] = 0;
										b_erase = false;
									}
									term_sgn[i2] = 0;
								} else {
									b_erase2 = true;
								}
							}
							if(b_erase2) {
								variable_count.erase(variable_count.begin() + i2);
								variables.erase(variables.begin() + i2);
								term_deg2.erase(term_deg2.begin() + i2);
								term_sgn.erase(term_sgn.begin() + i2);
							}
							break;
						}
					}
				}
			}
			if(b_erase) {
				variable_count.erase(variable_count.begin() + i);
				variables.erase(variables.begin() + i);
				term_deg2.erase(term_deg2.begin() + i);
				term_sgn.erase(term_sgn.begin() + i);
			} else if(variable_count[i] == mstruct.size()) {
				factorize_variable(mstruct, variables[i], term_deg2[i]);
				if(CALCULATOR->aborted()) return true;
				factorize_variables(mstruct, eo);
				return true;
			} else {
				i++;
			}
		}
		if(variables.size() == 1) {
			factorize_variable(mstruct, variables[0], term_deg2[0]);
			if(CALCULATOR->aborted()) return true;
			factorize_variables(mstruct, eo);
			return true;
		}
		Number uncertainty;
		size_t u_index = 0;
		for(size_t i = 0; i < variables.size(); i++) {
			const MathStructure *v_ms;
			Number nr;
			if(variables[i].isPower()) v_ms = &((KnownVariable*) variables[i][0].variable())->get();
			else v_ms = &((KnownVariable*) variables[i].variable())->get();
			if(v_ms->isNumber()) nr = v_ms->number();
			else if(v_ms->isMultiplication() && v_ms->size() > 0 && (*v_ms)[0].isNumber()) nr = (v_ms)[0].number();
			else {
				MathStructure mtest(*v_ms);
				mtest.calculatesub(eo, eo, true);
				if(mtest.isNumber()) nr = mtest.number();
				else if(mtest.isMultiplication() && mtest.size() > 0 && mtest[0].isNumber()) nr = mtest[0].number();
			}
			if(nr.isInterval()) {
				Number u_candidate(nr.uncertainty());
				if(variables[i].isPower() && variables[i][1].isNumber() && variables[i][1].number().isReal()) u_candidate.raise(variables[i][1].number());
				u_candidate.multiply(variable_count[i]);
				if(u_candidate.isGreaterThan(uncertainty)) {
					uncertainty = u_candidate;
					u_index = i;
				}
			}
		}
		if(!uncertainty.isZero()) {
			factorize_variable(mstruct, variables[u_index], term_deg2[u_index]);
			if(CALCULATOR->aborted()) return true;
			factorize_variables(mstruct, eo);
			return true;
		}
		
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(factorize_variables(mstruct[i], eo)) {
			mstruct.childUpdated(i + 1);
			b = true;
		}
		if(CALCULATOR->aborted()) return b;
	}
	return b;
}


MathStructure &MathStructure::eval(const EvaluationOptions &eo) {

	if(m_type == STRUCT_NUMBER) return *this;

	unformat(eo);
	
	EvaluationOptions feo = eo;
	feo.structuring = STRUCTURING_NONE;
	feo.do_polynomial_division = false;
	EvaluationOptions eo2 = eo;
	eo2.structuring = STRUCTURING_NONE;
	eo2.expand = false;
	eo2.test_comparisons = false;

	if(eo.calculate_functions && calculateFunctions(feo)) {
		unformat(eo);
	}

	if(m_type == STRUCT_NUMBER) return *this;

	if(eo2.approximation == APPROXIMATION_TRY_EXACT || (eo2.approximation == APPROXIMATION_APPROXIMATE && (containsUnknowns() || containsInterval(false, true, false)))) {
		EvaluationOptions eo3 = eo2;
		if(eo.approximation == APPROXIMATION_APPROXIMATE && !containsUnknowns()) eo3.approximation = APPROXIMATION_EXACT_VARIABLES;
		else eo3.approximation = APPROXIMATION_EXACT;
		eo3.split_squares = false;
		eo3.assume_denominators_nonzero = false;
		calculatesub(eo3, feo);
		if(m_type == STRUCT_NUMBER) return *this;
		if(!CALCULATOR->usesIntervalArithmetic() && eo.expand != 0 && !containsType(STRUCT_COMPARISON, true, true, true)) {
			unformat(eo);
			eo3.expand = -1;
			calculatesub(eo3, feo);
		}
		eo3.approximation = APPROXIMATION_APPROXIMATE;
		factorize_variables(*this, eo3);
		eo2.approximation = APPROXIMATION_APPROXIMATE;
	}

	calculatesub(eo2, feo);
	if(m_type == STRUCT_NUMBER || CALCULATOR->aborted()) return *this;

	eo2.sync_units = false;
	
	if(eo2.isolate_x) {
		eo2.assume_denominators_nonzero = false;
		if(isolate_x(eo2, feo)) {
			if(CALCULATOR->aborted()) return *this;
			if(eo.assume_denominators_nonzero) eo2.assume_denominators_nonzero = 2;
			calculatesub(eo2, feo);
		} else {
			if(eo.assume_denominators_nonzero) eo2.assume_denominators_nonzero = 2;
		}
		if(CALCULATOR->aborted()) return *this;
	}

	if(eo.expand != 0 || (eo.test_comparisons && containsType(STRUCT_COMPARISON))) {
		eo2.test_comparisons = eo.test_comparisons;
		eo2.expand = eo.expand;
		if(eo2.expand && (!eo.test_comparisons || !containsType(STRUCT_COMPARISON))) eo2.expand = -2;
		bool b = eo2.test_comparisons;
		if(!b && isAddition()) {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).containsType(STRUCT_ADDITION, false) == 1) {
					b = true;
					break;
				}
			}
		} else if(!b) {
			b = containsType(STRUCT_ADDITION, false) == 1;
		}
		if(b) {
			calculatesub(eo2, feo);
			if(CALCULATOR->aborted()) return *this;

			if(eo.do_polynomial_division) do_simplification(*this, eo2, true, eo.structuring == STRUCTURING_NONE || eo.structuring == STRUCTURING_FACTORIZE, false, true, true);
			if(CALCULATOR->aborted()) return *this;
			if(eo2.isolate_x) {
				eo2.assume_denominators_nonzero = false;
				if(isolate_x(eo2, feo)) {
					if(CALCULATOR->aborted()) return *this;
					if(eo.assume_denominators_nonzero) eo2.assume_denominators_nonzero = 2;
					calculatesub(eo2, feo);
					if(containsType(STRUCT_ADDITION, false) == 1 && eo.do_polynomial_division) do_simplification(*this, eo2, true, eo.structuring == STRUCTURING_NONE || eo.structuring == STRUCTURING_FACTORIZE, false, true, true);
				} else {
					if(eo.assume_denominators_nonzero) eo2.assume_denominators_nonzero = 2;
				}
				if(CALCULATOR->aborted()) return *this;
			}
		}
	}
	if(eo2.isolate_x && containsType(STRUCT_COMPARISON) && eo2.assume_denominators_nonzero) {
		eo2.assume_denominators_nonzero = 2;
		if(try_isolate_x(*this, eo2, feo)) {
			if(CALCULATOR->aborted()) return *this;
			calculatesub(eo2, feo);
			if(containsType(STRUCT_ADDITION, false) == 1 && eo.do_polynomial_division) do_simplification(*this, eo2, true, eo.structuring == STRUCTURING_NONE || eo.structuring == STRUCTURING_FACTORIZE, false, true, true);
		}
	}

	if(CALCULATOR->aborted()) return *this;
	
	structure(eo.structuring, eo2, false);

	clean_multiplications(*this);

	return *this;
}

bool factorize_find_multiplier(const MathStructure &mstruct, MathStructure &mnew, MathStructure &factor_mstruct, bool only_units = false) {
	factor_mstruct.set(m_one);
	switch(mstruct.type()) {
		case STRUCT_ADDITION: {
			if(!only_units) {
				bool bfrac = false, bint = true;
				idm1(mstruct, bfrac, bint);
				if(bfrac || bint) {
					Number gcd(1, 1);
					idm2(mstruct, bfrac, bint, gcd);
					if((bint || bfrac) && !gcd.isOne()) {
						if(bfrac) gcd.recip();
						factor_mstruct.set(gcd);
					}
				}
			}
			if(mstruct.size() > 0) {
				size_t i = 0;
				const MathStructure *cur_mstruct;
				while(true) {
					if(mstruct[0].isMultiplication()) {
						if(i >= mstruct[0].size()) {
							break;
						}
						cur_mstruct = &mstruct[0][i];
					} else {
						cur_mstruct = &mstruct[0];
					}
					if(!cur_mstruct->isNumber() && (!only_units || cur_mstruct->isUnit_exp())) {
						const MathStructure *exp = NULL;
						const MathStructure *bas;
						if(cur_mstruct->isPower() && IS_REAL((*cur_mstruct)[1]) && !(*cur_mstruct)[0].isNumber()) {
							exp = cur_mstruct->exponent();
							bas = cur_mstruct->base();
						} else {
							bas = cur_mstruct;
						}
						bool b = true;
						for(size_t i2 = 1; i2 < mstruct.size(); i2++) {
							b = false;
							size_t i3 = 0;
							const MathStructure *cmp_mstruct;
							while(true) {
								if(mstruct[i2].isMultiplication()) {
									if(i3 >= mstruct[i2].size()) {
										break;
									}
									cmp_mstruct = &mstruct[i2][i3];
								} else {
									cmp_mstruct = &mstruct[i2];
								}
								if(cmp_mstruct->equals(*bas)) {
									if(exp) {
										exp = NULL;
									}
									b = true;
									break;
								} else if(cmp_mstruct->isPower() && cmp_mstruct->base()->equals(*bas)) {
									if(exp) {
										if(IS_REAL((*cmp_mstruct)[1])) {
											if(cmp_mstruct->exponent()->number().isLessThan(exp->number())) {
												exp = cmp_mstruct->exponent();
											}
											b = true;
											break;
										} else {
											exp = NULL;
										}
									} else {
										b = true;
										break;
									}
								}
								if(!mstruct[i2].isMultiplication()) {
									break;
								}
								i3++;
							}
							if(!b) break;
						}
						if(b) {
							if(exp) {
								MathStructure *mstruct = new MathStructure(*bas);
								mstruct->raise(*exp);
								if(factor_mstruct.isOne()) {
									factor_mstruct.set_nocopy(*mstruct);
									mstruct->unref();
								} else {
									factor_mstruct.multiply_nocopy(mstruct, true);
								}
							} else {
								if(factor_mstruct.isOne()) factor_mstruct.set(*bas);
								else factor_mstruct.multiply(*bas, true);
							}
						}
					}
					if(!mstruct[0].isMultiplication()) {
						break;
					}
					i++;
				}
			}
			if(!factor_mstruct.isOne()) {
				if(&mstruct != &mnew) mnew.set(mstruct);
				MathStructure *mfactor;
				size_t i = 0;
				while(true) {
					if(factor_mstruct.isMultiplication()) {
						if(i >= factor_mstruct.size()) break;
						mfactor = &factor_mstruct[i];
					} else {
						mfactor = &factor_mstruct;
					}
					for(size_t i2 = 0; i2 < mnew.size(); i2++) {
						switch(mnew[i2].type()) {
							case STRUCT_NUMBER: {
								if(mfactor->isNumber()) {
									mnew[i2].number() /= mfactor->number();
								}
								break;
							}
							case STRUCT_POWER: {
								if(!IS_REAL(mnew[i2][1])) {
									if(mfactor->isNumber()) {
										mnew[i2].transform(STRUCT_MULTIPLICATION);
										mnew[i2].insertChild(MathStructure(1, 1, 0), 1);
										mnew[i2][0].number() /= mfactor->number();
									} else {
										mnew[i2].set(m_one);
									}
								} else if(mfactor->isNumber()) {
									mnew[i2].transform(STRUCT_MULTIPLICATION);
									mnew[i2].insertChild(MathStructure(1, 1, 0), 1);
									mnew[i2][0].number() /= mfactor->number();
								} else if(mfactor->isPower() && IS_REAL((*mfactor)[1])) {
									if(mfactor->equals(mnew[i2])) {
										mnew[i2].set(m_one);
									} else {
										mnew[i2][1].number() -= mfactor->exponent()->number();
										if(mnew[i2][1].number().isOne()) {
											mnew[i2].setToChild(1, true);
										}
									}
								} else {
									mnew[i2][1].number() -= 1;
									if(mnew[i2][1].number().isOne()) {
										mnew[i2].setToChild(1);
									} else if(mnew[i2][1].number().isZero()) {
										mnew[i2].set(m_one);
									}
								}
								break;
							}
							case STRUCT_MULTIPLICATION: {
								bool b = true;
								if(mfactor->isNumber() && (mnew[i2].size() < 1 || !mnew[i2][0].isNumber())) {
									mnew[i2].insertChild(MathStructure(1, 1, 0), 1);
								}
								for(size_t i3 = 0; i3 < mnew[i2].size() && b; i3++) {
									switch(mnew[i2][i3].type()) {
										case STRUCT_NUMBER: {
											if(mfactor->isNumber()) {
												if(mfactor->equals(mnew[i2][i3])) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3].number() /= mfactor->number();
												}
												b = false;
											}
											break;
										}
										case STRUCT_POWER: {
											if(!IS_REAL(mnew[i2][i3][1])) {
												if(mfactor->equals(mnew[i2][i3])) {
													mnew[i2].delChild(i3 + 1);
													b = false;
												}
											} else if(mfactor->isPower() && IS_REAL((*mfactor)[1]) && mfactor->base()->equals(mnew[i2][i3][0])) {
												if(mfactor->equals(mnew[i2][i3])) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3][1].number() -= mfactor->exponent()->number();
													if(mnew[i2][i3][1].number().isOne()) {
														MathStructure mstruct2(mnew[i2][i3][0]);
														mnew[i2][i3] = mstruct2;
													} else if(mnew[i2][i3][1].number().isZero()) {
														mnew[i2].delChild(i3 + 1);
													}
												}
												b = false;
											} else if(mfactor->equals(mnew[i2][i3][0])) {
												if(mnew[i2][i3][1].number() == 2) {
													MathStructure mstruct2(mnew[i2][i3][0]);
													mnew[i2][i3] = mstruct2;
												} else if(mnew[i2][i3][1].number().isOne()) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3][1].number() -= 1;
												}
												b = false;												
											}	
											break;										
										}
										default: {
											if(mfactor->equals(mnew[i2][i3])) {
												mnew[i2].delChild(i3 + 1);
												b = false;
											}
										}
									}
								}
								if(mnew[i2].size() == 1) {
									MathStructure mstruct2(mnew[i2][0]);
									mnew[i2] = mstruct2;
								}
								break;
							}
							default: {
								if(mfactor->isNumber()) {
									mnew[i2].transform(STRUCT_MULTIPLICATION);
									mnew[i2].insertChild(MathStructure(1, 1, 0), 1);
									mnew[i2][0].number() /= mfactor->number();
								} else {
									mnew[i2].set(m_one);
								}
							}
						}
					}
					if(factor_mstruct.isMultiplication()) {
						i++;
					} else {
						break;
					}
				}
				return true;
			}
		}
		default: {}
	}
	return false;
}

bool get_first_symbol(const MathStructure &mpoly, MathStructure &xvar) {
	if(IS_A_SYMBOL(mpoly) || mpoly.isUnit()) {
		xvar = mpoly;
		return true;
	} else if(mpoly.isAddition() || mpoly.isMultiplication()) {
		for(size_t i = 0; i < mpoly.size(); i++) {
			if(get_first_symbol(mpoly[i], xvar)) return true;
		}
	} else if(mpoly.isPower()) {
		return get_first_symbol(mpoly[0], xvar);
	}
	return false;
}

bool MathStructure::polynomialDivide(const MathStructure &mnum, const MathStructure &mden, MathStructure &mquotient, const EvaluationOptions &eo, bool check_args) {

	mquotient.clear();
	
	if(CALCULATOR->aborted()) return false;

	if(mden.isZero()) {
		//division by zero
		return false;
	}
	if(mnum.isZero()) {
		mquotient.clear();
		return true;
	}
	if(mden.isNumber()) {
		mquotient = mnum;
		if(mnum.isNumber()) {
			mquotient.number() /= mden.number();
		} else {
			mquotient.calculateDivide(mden, eo);
		}
		return true;
	} else if(mnum.isNumber()) {
		return false;
	}

	if(mnum == mden) {
		mquotient.set(1, 1, 0);
		return true;
	}

	if(check_args && (!mnum.isRationalPolynomial() || !mden.isRationalPolynomial())) {
		return false;
	}
	

	MathStructure xvar;
	if(!get_first_symbol(mnum, xvar) && !get_first_symbol(mden, xvar)) return false;
	
	EvaluationOptions eo2 = eo;
	eo2.keep_zero_units = false;

	Number numdeg = mnum.degree(xvar);
	Number dendeg = mden.degree(xvar);
	MathStructure dencoeff;
	mden.coefficient(xvar, dendeg, dencoeff);

	MathStructure mrem(mnum);
	
	while(numdeg.isGreaterThanOrEqualTo(dendeg)) {
		if(CALCULATOR->aborted()) return false;
		MathStructure numcoeff;
		mrem.coefficient(xvar, numdeg, numcoeff);
		numdeg -= dendeg;
		if(numcoeff == dencoeff) {
			if(numdeg.isZero()) {
				numcoeff.set(1, 1, 0);
			} else {
				numcoeff = xvar;
				if(!numdeg.isOne()) {
					numcoeff.raise(numdeg);
				}
			}
		} else {
			if(dencoeff.isNumber()) {
				if(numcoeff.isNumber()) {
					numcoeff.number() /= dencoeff.number();
				} else {
					numcoeff.calculateDivide(dencoeff, eo2);
				}
			} else {
				MathStructure mcopy(numcoeff);
				if(!MathStructure::polynomialDivide(mcopy, dencoeff, numcoeff, eo2, false)) {
					return false;
				}
			}
			if(!numdeg.isZero() && !numcoeff.isZero()) {
				if(numcoeff.isOne()) {
					numcoeff = xvar;
					if(!numdeg.isOne()) {
						numcoeff.raise(numdeg);
					}
				} else {
					numcoeff.multiply(xvar, true);
					if(!numdeg.isOne()) {
						numcoeff[numcoeff.size() - 1].raise(numdeg);
					}
					numcoeff.calculateMultiplyLast(eo2);
				}
			}
		}
		if(mquotient.isZero()) mquotient = numcoeff;
		else mquotient.add(numcoeff, true);
		numcoeff.calculateMultiply(mden, eo2);
		mrem.calculateSubtract(numcoeff, eo2);
		if(mrem.isZero()) return true;
		numdeg = mrem.degree(xvar);
	}
	return false;
}

bool divide_in_z(const MathStructure &mnum, const MathStructure &mden, MathStructure &mquotient, const sym_desc_vec &sym_stats, size_t var_i, const EvaluationOptions &eo) {
	
	if(var_i >= sym_stats.size()) return false;
	
	mquotient.clear();
	if(mden.isZero()) return false;
	if(mnum.isZero()) return true;
	if(mden.isOne()) {
		mquotient = mnum;
		return true;
	}
	if(mnum.isNumber()) {
		if(!mden.isNumber()) {
			return false;
		}
		mquotient = mnum;
		return mquotient.number().divide(mden.number()) && mquotient.isInteger();
	}
	if(mnum == mden) {
		mquotient.set(1, 1, 0);
		return true;
	}
	
	if(mden.isPower()) {
		MathStructure qbar(mnum);
		for(Number ni(mden[1].number()); ni.isPositive(); ni--) {
			if(!divide_in_z(qbar, mden[0], mquotient, sym_stats, var_i, eo)) return false;
			qbar = mquotient;
		}
		return true;
	}

	if(mden.isMultiplication()) {
		MathStructure qbar(mnum);
		for(size_t i = 0; i < mden.size(); i++) {
			sym_desc_vec sym_stats2;
			get_symbol_stats(mnum, mden[i], sym_stats2);
			if(!divide_in_z(qbar, mden[i], mquotient, sym_stats2, 0, eo)) return false;
			qbar = mquotient;
		}
		return true;
	}	
	
	const MathStructure &xvar = sym_stats[var_i].sym;

	Number numdeg = mnum.degree(xvar);
	Number dendeg = mden.degree(xvar);
	if(dendeg.isGreaterThan(numdeg)) return false;
	MathStructure dencoeff;
	MathStructure mrem(mnum);
	mden.coefficient(xvar, dendeg, dencoeff);
	while(numdeg.isGreaterThanOrEqualTo(dendeg)) {
		MathStructure numcoeff;
		mrem.coefficient(xvar, numdeg, numcoeff);
		MathStructure term;
		if(!divide_in_z(numcoeff, dencoeff, term, sym_stats, var_i + 1, eo)) break;
		numdeg -= dendeg;
		if(!numdeg.isZero() && !term.isZero()) {
			if(term.isOne()) {
				term = xvar;		
				if(!numdeg.isOne()) {
					term.raise(numdeg);
				}
			} else {
				term.multiply(xvar, true);		
				if(!numdeg.isOne()) {
					term[term.size() - 1].raise(numdeg);
				}
				term.calculateMultiplyLast(eo);
			}
		}
		if(mquotient.isZero()) {
			mquotient = term;
		} else {
			mquotient.calculateAdd(term, eo);
		}
		term.calculateMultiply(mden, eo);
		mrem.calculateSubtract(term, eo);
		if(mrem.isZero()) {
			return true;
		}
		numdeg = mrem.degree(xvar);
	}
	return false;
}

bool prem(const MathStructure &mnum, const MathStructure &mden, const MathStructure &xvar, MathStructure &mrem, const EvaluationOptions &eo, bool check_args) {

	mrem.clear();
	if(mden.isZero()) {
		//division by zero
		return false;
	}
	if(mnum.isNumber()) {
		if(!mden.isNumber()) {
			mrem = mden;
		}
		return true;
	}
	if(check_args && (!mnum.isRationalPolynomial() || !mden.isRationalPolynomial())) {
		return false;
	}

	mrem = mnum;
	MathStructure eb(mden);
	Number rdeg = mrem.degree(xvar);
	Number bdeg = eb.degree(xvar);
	MathStructure blcoeff;
	if(bdeg.isLessThanOrEqualTo(rdeg)) {
		eb.coefficient(xvar, bdeg, blcoeff);
		if(bdeg == 0) {
			eb.clear();
		} else {
			MathStructure mpow(xvar);
			mpow.raise(bdeg);
			mpow.calculateRaiseExponent(eo);
			//POWER_CLEAN(mpow)
			mpow.calculateMultiply(blcoeff, eo);
			eb.calculateSubtract(mpow, eo);
		}
	} else {
		blcoeff.set(1, 1, 0);
	}

	Number delta(rdeg);
	delta -= bdeg;
	delta++;
	int i = 0;
	while(rdeg.isGreaterThanOrEqualTo(bdeg) && !mrem.isZero()) {
		MathStructure rlcoeff;
		mrem.coefficient(xvar, rdeg, rlcoeff);
		MathStructure term(xvar);
		term.raise(rdeg);
		term[1].number() -= bdeg;
		term.calculateRaiseExponent(eo);
		//POWER_CLEAN(term)
		term.calculateMultiply(rlcoeff, eo);
		term.calculateMultiply(eb, eo);
		if(rdeg == 0) {
			mrem = term;
			mrem.calculateNegate(eo);
		} else {
			if(!rdeg.isZero()) {
				rlcoeff.multiply(xvar, true);
				if(!rdeg.isOne()) {
					rlcoeff[rlcoeff.size() - 1].raise(rdeg);
					rlcoeff[rlcoeff.size() - 1].calculateRaiseExponent(eo);
				}
				rlcoeff.calculateMultiplyLast(eo);
			}
			mrem.calculateSubtract(rlcoeff, eo);
			mrem.calculateMultiply(blcoeff, eo);
			mrem.calculateSubtract(term, eo);
		}
		rdeg = mrem.degree(xvar);
		i++;
	}
	delta -= i;
	blcoeff.raise(delta);
	blcoeff.calculateRaiseExponent(eo);
	mrem.calculateMultiply(blcoeff, eo);
	return true;
}


bool sr_gcd(const MathStructure &m1, const MathStructure &m2, MathStructure &mgcd, const sym_desc_vec &sym_stats, size_t var_i, const EvaluationOptions &eo) {

	if(var_i >= sym_stats.size()) return false;
	const MathStructure &xvar = sym_stats[var_i].sym;
	
	MathStructure c, d;
	Number adeg = m1.degree(xvar);
	Number bdeg = m2.degree(xvar);
	Number cdeg, ddeg;
	if(adeg.isGreaterThanOrEqualTo(bdeg)) {
		c = m1;
		d = m2;
		cdeg = adeg;
		ddeg = bdeg;
	} else {
		c = m2;
		d = m1;
		cdeg = bdeg;
		ddeg = adeg;
	}
	
	MathStructure cont_c, cont_d;
	c.polynomialContent(xvar, cont_c, eo);
	d.polynomialContent(xvar, cont_d, eo);

	MathStructure gamma;
	if(!MathStructure::gcd(cont_c, cont_d, gamma, eo, NULL, NULL, false)) return false;
	mgcd = gamma;
	if(ddeg.isZero()) {
		return true;
	}
	MathStructure prim_c, prim_d;	
	c.polynomialPrimpart(xvar, cont_c, prim_c, eo);
	d.polynomialPrimpart(xvar, cont_d, prim_d, eo);
	c = prim_c;
	d = prim_d;
	
	MathStructure r;
	MathStructure ri(1, 1, 0);
	MathStructure psi(1, 1, 0);
	Number delta(cdeg);
	delta -= ddeg;

	while(true) {
	
		if(CALCULATOR->aborted()) return false;

		prem(c, d, xvar, r, eo, false);
		if(r.isZero()) {
			mgcd = gamma;
			MathStructure mprim;
			d.polynomialPrimpart(xvar, mprim, eo);
			if(CALCULATOR->aborted()) return false;
			mgcd.calculateMultiply(mprim, eo);
			return true;
		}

		c = d;
		cdeg = ddeg;
		
		MathStructure psi_pow(psi);
		psi_pow.calculateRaise(delta, eo);
		ri.calculateMultiply(psi_pow, eo);
		if(!divide_in_z(r, ri, d, sym_stats, var_i, eo)) {
			return false;
		}
		ddeg = d.degree(xvar);
		if(ddeg.isZero()) {
			if(r.isNumber()) {
				mgcd = gamma;
			} else {
				r.polynomialPrimpart(xvar, mgcd, eo);
				if(CALCULATOR->aborted()) return false;
				mgcd.calculateMultiply(gamma, eo);
			}
			return true;
		}

		c.lcoefficient(xvar, ri);
		if(delta.isOne()) {
			psi = ri;
		} else if(!delta.isZero()) {
			MathStructure ri_pow(ri);
			ri_pow.calculateRaise(delta, eo);
			MathStructure psi_pow(psi);
			delta--;
			psi_pow.calculateRaise(delta, eo);
			divide_in_z(ri_pow, psi_pow, psi, sym_stats, var_i + 1, eo);
		}
		delta = cdeg;
		delta -= ddeg;
		
	}
	
	return false;
	
}

Number MathStructure::maxCoefficient() {
	if(isNumber()) {
		Number nr(o_number);
		nr.abs();
		return nr;
	} else if(isAddition()) {
		Number cur_max(overallCoefficient());
		cur_max.abs();
		for(size_t i = 0; i < SIZE; i++) {
			Number a(CHILD(i).overallCoefficient());
			a.abs();
			if(a.isGreaterThan(cur_max)) cur_max = a;
		}
		return cur_max;
	} else if(isMultiplication()) {
		Number nr(overallCoefficient());
		nr.abs();
		return nr;
	} else {
		return nr_one;
	}
}
void polynomial_smod(const MathStructure &mpoly, const Number &xi, MathStructure &msmod, const EvaluationOptions &eo, MathStructure *mparent, size_t index_smod) {
	if(mpoly.isNumber()) {
		msmod = mpoly;
		msmod.number().smod(xi);
	} else if(mpoly.isAddition()) {
		msmod.clear();
		msmod.setType(STRUCT_ADDITION);
		msmod.resizeVector(mpoly.size(), m_zero);
		for(size_t i = 0; i < mpoly.size(); i++) {
			polynomial_smod(mpoly[i], xi, msmod[i], eo, &msmod, i);
		}
		msmod.calculatesub(eo, eo, false, mparent, index_smod);
	} else if(mpoly.isMultiplication()) {
		msmod = mpoly;
		if(msmod.size() > 0 && msmod[0].isNumber()) {
			if(!msmod[0].number().smod(xi) || msmod[0].isZero()) {
				msmod.clear();
			}
		}
	} else {
		msmod = mpoly;
	}
}

void interpolate(const MathStructure &gamma, const Number &xi, const MathStructure &xvar, MathStructure &minterp, const EvaluationOptions &eo) {
	MathStructure e(gamma);
	Number rxi(xi);
	rxi.recip();
	minterp.clear();
	for(long int i = 0; !e.isZero(); i++) {
		MathStructure gi;
		polynomial_smod(e, xi, gi, eo);
		if(minterp.isZero() && !gi.isZero()) {
			minterp = gi;
			if(i != 0) {
				if(minterp.isOne()) {
					minterp = xvar;
					if(i != 1) minterp.raise(i);
				} else {
					minterp.multiply(xvar, true);
					if(i != 1) minterp[minterp.size() - 1].raise(i);
					minterp.calculateMultiplyLast(eo);
				}
			}
		} else if(!gi.isZero()) {
			minterp.add(gi, true);
			if(i != 0) {
				if(minterp[minterp.size() - 1].isOne()) {
					minterp[minterp.size() - 1] = xvar;
					if(i != 1) minterp[minterp.size() - 1].raise(i);
				} else {
					minterp[minterp.size() - 1].multiply(xvar, true);
					if(i != 1) minterp[minterp.size() - 1][minterp[minterp.size() - 1].size() - 1].raise(i);
					minterp[minterp.size() - 1].calculateMultiplyLast(eo);
				}
			}
		}
		if(!gi.isZero()) e.calculateSubtract(gi, eo);
		e.calculateMultiply(rxi, eo);
	}
	minterp.calculatesub(eo, eo, false);
}

bool heur_gcd(const MathStructure &m1, const MathStructure &m2, MathStructure &mgcd, const EvaluationOptions &eo, MathStructure *ca, MathStructure *cb, sym_desc_vec &sym_stats, size_t var_i) {
	if(var_i >= sym_stats.size()) return false;
	if(m1.isZero() || m2.isZero())	return false;

	if(m1.isNumber() && m2.isNumber()) {
		mgcd = m1;
		if(!mgcd.number().gcd(m2.number())) mgcd.set(1, 1, 0);
		if(ca) {
			*ca = m1;
			ca->number() /= mgcd.number();
		}
		if(cb) {
			*cb = m2;
			cb->number() /= mgcd.number();
		}
		return true;
	}

	const MathStructure &xvar = sym_stats[var_i].sym;
	Number nr_gc;
	integer_content(m1, nr_gc);
	Number nr_rgc;
	integer_content(m2, nr_rgc);
	nr_gc.gcd(nr_rgc);
	nr_rgc = nr_gc;
	nr_rgc.recip();
	MathStructure p(m1);
	p.calculateMultiply(nr_rgc, eo);
	MathStructure q(m2);	
	q.calculateMultiply(nr_rgc, eo);
	Number maxdeg(p.degree(xvar));
	Number maxdeg2(q.degree(xvar));
	if(maxdeg2.isGreaterThan(maxdeg)) maxdeg = maxdeg2;
	Number mp(p.maxCoefficient());
	Number mq(q.maxCoefficient());
	Number xi;
	if(mp.isGreaterThan(mq)) {
		xi = mq;
	} else {
		xi = mp;
	}
	xi *= 2;
	xi += 2;
	
	for(int t = 0; t < 6; t++) {
	
		if(CALCULATOR->aborted()) return false;

		if((maxdeg * xi.integerLength()).isGreaterThan(100000L)) {
			return false;
		}

		MathStructure cp, cq;
		MathStructure gamma;
		MathStructure psub(p);
		psub.calculateReplace(xvar, xi, eo);
		MathStructure qsub(q);
		qsub.calculateReplace(xvar, xi, eo);

		if(heur_gcd(psub, qsub, gamma, eo, &cp, &cq, sym_stats, var_i + 1)) {

			interpolate(gamma, xi, xvar, mgcd, eo);

			Number ig;
			integer_content(mgcd, ig);
			ig.recip();
			mgcd.calculateMultiply(ig, eo); 

			MathStructure dummy;
			if(divide_in_z(p, mgcd, ca ? *ca : dummy, sym_stats, var_i, eo) && divide_in_z(q, mgcd, cb ? *cb : dummy, sym_stats, var_i, eo)) {
				mgcd.calculateMultiply(nr_gc, eo);
				return true;
			}
		}

		Number xi2(xi);
		xi2.isqrt();
		xi2.isqrt();
		xi *= xi2;
		xi *= 73794L;
		xi.iquo(27011L);
		
	}

	return false;
	
}

int MathStructure::polynomialUnit(const MathStructure &xvar) const {
	MathStructure coeff;
	lcoefficient(xvar, coeff);	
	if(coeff.hasNegativeSign()) return -1;
	return 1;
}

void integer_content(const MathStructure &mpoly, Number &icontent) {
	if(mpoly.isNumber()) {
		icontent = mpoly.number();
		icontent.abs();
	} else if(mpoly.isAddition()) {
		icontent.clear();
		Number l(1, 1);
		for(size_t i = 0; i < mpoly.size(); i++) {
			if(mpoly[i].isNumber()) {
				if(!icontent.isOne()) {
					Number c = icontent;				
					icontent = mpoly[i].number().numerator();
					icontent.gcd(c);
				}
				Number l2 = l;
				l = mpoly[i].number().denominator();
				l.lcm(l2);
			} else if(mpoly[i].isMultiplication()) {
				if(!icontent.isOne()) {
					Number c = icontent;				
					icontent = mpoly[i].overallCoefficient().numerator();
					icontent.gcd(c);
				}
				Number l2 = l;
				l = mpoly[i].overallCoefficient().denominator();
				l.lcm(l2);
			} else {
				icontent.set(1, 1, 0);
			}
		}
		icontent /= l;
	} else if(mpoly.isMultiplication()) {
		icontent = mpoly.overallCoefficient();
		icontent.abs();
	} else {
		icontent.set(1, 1, 0);
	}
}

bool MathStructure::lcm(const MathStructure &m1, const MathStructure &m2, MathStructure &mlcm, const EvaluationOptions &eo, bool check_args) {
	if(m1.isNumber() && m2.isNumber()) {
		mlcm = m1;
		return mlcm.number().lcm(m2.number());
	}
	if(check_args && (!m1.isRationalPolynomial() || !m2.isRationalPolynomial())) {
		return false;
	}
	MathStructure ca, cb;
	if(!MathStructure::gcd(m1, m2, mlcm, eo, &ca, &cb, false)) return false;
	mlcm.calculateMultiply(ca, eo);
	mlcm.calculateMultiply(cb, eo);
	return true;
}

void MathStructure::polynomialContent(const MathStructure &xvar, MathStructure &mcontent, const EvaluationOptions &eo) const {
	if(isZero()) {
		mcontent.clear();
		return;
	}
	if(isNumber()) {
		mcontent = *this;
		mcontent.number().setNegative(false);
		return;
	}
	MathStructure c;
	integer_content(*this, c.number());
	MathStructure r(*this);
	if(!c.isOne()) r.calculateDivide(c, eo);
	MathStructure lcoeff;
	r.lcoefficient(xvar, lcoeff);
	if(lcoeff.isInteger()) {
		mcontent = c;
		return;
	}
	Number deg(r.degree(xvar));
	Number ldeg(r.ldegree(xvar));
	if(deg == ldeg) {
		mcontent = lcoeff;
		if(lcoeff.polynomialUnit(xvar) == -1) {
			c.number().negate();
		}
		mcontent.calculateMultiply(c, eo);
		return;
	}
	mcontent.clear();
	MathStructure mtmp, coeff;
	for(Number i(ldeg); i.isLessThanOrEqualTo(deg); i++) {
		coefficient(xvar, i, coeff);
		mtmp = mcontent;
		if(!MathStructure::gcd(coeff, mtmp, mcontent, eo, NULL, NULL, false)) mcontent.set(1, 1, 0);
		if(mcontent.isOne()) break;
	}
	if(!c.isOne()) mcontent.calculateMultiply(c, eo);

}

void MathStructure::polynomialPrimpart(const MathStructure &xvar, MathStructure &mprim, const EvaluationOptions &eo) const {
	if(isZero()) {
		mprim.clear();
		return;
	}
	if(isNumber()) {
		mprim.set(1, 1, 0);
		return;
	}

	MathStructure c;
	polynomialContent(xvar, c, eo);
	if(c.isZero()) {
		mprim.clear();
		return;
	}
	bool b = (polynomialUnit(xvar) == -1);
	if(c.isNumber()) {
		if(b) c.number().negate();
		mprim = *this;
		mprim.calculateDivide(c, eo);
		return;
	}
	if(b) c.calculateNegate(eo);
	MathStructure::polynomialQuotient(*this, c, xvar, mprim, eo, false);
}
void MathStructure::polynomialUnitContentPrimpart(const MathStructure &xvar, int &munit, MathStructure &mcontent, MathStructure &mprim, const EvaluationOptions &eo) const {

	if(isZero()) {
		munit = 1;
		mcontent.clear();
		mprim.clear();
		return;
	}

	if(isNumber()) {
		if(o_number.isNegative()) {
			munit = -1;
			mcontent = *this;
			mcontent.number().abs();
		} else {
			munit = 1;
			mcontent = *this;
		}
		mprim.set(1, 1, 0);
		return;
	}

	munit = polynomialUnit(xvar);
	polynomialContent(xvar, mcontent, eo);
	if(mcontent.isZero()) {
		mprim.clear();
		return;
	}
	if(mcontent.isNumber()) {
		mprim = *this;
		if(munit == -1) {
			Number c(mcontent.number());
			c.negate();
			mprim.calculateDivide(c, eo);
		} else {
			mprim.calculateDivide(mcontent, eo);
		}
		return;
	}
	if(munit == -1) {
		MathStructure c(mcontent);
		c.calculateNegate(eo);
		MathStructure::polynomialQuotient(*this, c, xvar, mprim, eo, false);
	} else {
		MathStructure::polynomialQuotient(*this, mcontent, xvar, mprim, eo, false);
	}
	
}


void MathStructure::polynomialPrimpart(const MathStructure &xvar, const MathStructure &c, MathStructure &mprim, const EvaluationOptions &eo) const {
	if(isZero() || c.isZero()) {
		mprim.clear();
		return;
	}
	if(isNumber()) {
		mprim.set(1, 1, 0);
		return;
	}
	bool b = (polynomialUnit(xvar) == -1);
	if(c.isNumber()) {
		MathStructure cn(c);
		if(b) cn.number().negate();
		mprim = *this;
		mprim.calculateDivide(cn, eo);
		return;
	}
	if(b) {
		MathStructure cn(c);
		cn.calculateNegate(eo);
		MathStructure::polynomialQuotient(*this, cn, xvar, mprim, eo, false);
	} else {
		MathStructure::polynomialQuotient(*this, c, xvar, mprim, eo, false);
	}
}

const Number& MathStructure::degree(const MathStructure &xvar) const {
	const Number *c = NULL;
	const MathStructure *mcur = NULL;
	for(size_t i = 0; ; i++) {
		if(isAddition()) {
			if(i >= SIZE) break;
			mcur = &CHILD(i);
		} else {
			mcur = this;
		}
		if((*mcur) == xvar) {
			if(!c) {
				c = &nr_one;
			}
		} else if(mcur->isPower() && (*mcur)[0] == xvar && (*mcur)[1].isNumber()) {
			if(!c || c->isLessThan((*mcur)[1].number())) {
				c = &(*mcur)[1].number();
			}
		} else if(mcur->isMultiplication()) {
			for(size_t i2 = 0; i2 < mcur->size(); i2++) {
				if((*mcur)[i2] == xvar) {
					if(!c) {
						c = &nr_one;
					}
				} else if((*mcur)[i2].isPower() && (*mcur)[i2][0] == xvar && (*mcur)[i2][1].isNumber()) {
					if(!c || c->isLessThan((*mcur)[i2][1].number())) {
						c = &(*mcur)[i2][1].number();
					}
				}
			}
		}
		if(!isAddition()) break;
	}
	if(!c) return nr_zero;
	return *c;
}
const Number& MathStructure::ldegree(const MathStructure &xvar) const {
	const Number *c = NULL;
	const MathStructure *mcur = NULL;
	for(size_t i = 0; ; i++) {
		if(isAddition()) {
			if(i >= SIZE) break;
			mcur = &CHILD(i);
		} else {
			mcur = this;
		}
		if((*mcur) == xvar) {
			c = &nr_one;
		} else if(mcur->isPower() && (*mcur)[0] == xvar && (*mcur)[1].isNumber()) {
			if(!c || c->isGreaterThan((*mcur)[1].number())) {
				c = &(*mcur)[1].number();
			}
		} else if(mcur->isMultiplication()) {
			bool b = false;
			for(size_t i2 = 0; i2 < mcur->size(); i2++) {
				if((*mcur)[i2] == xvar) {
					c = &nr_one;
					b = true;					
				} else if((*mcur)[i2].isPower() && (*mcur)[i2][0] == xvar && (*mcur)[i2][1].isNumber()) {
					if(!c || c->isGreaterThan((*mcur)[i2][1].number())) {
						c = &(*mcur)[i2][1].number();
					}
					b = true;
				}
			}
			if(!b) return nr_zero;
		} else {
			return nr_zero;
		}
		if(!isAddition()) break;
	}
	if(!c) return nr_zero;
	return *c;
}
void MathStructure::lcoefficient(const MathStructure &xvar, MathStructure &mcoeff) const {
	coefficient(xvar, degree(xvar), mcoeff);
}
void MathStructure::tcoefficient(const MathStructure &xvar, MathStructure &mcoeff) const {
	coefficient(xvar, ldegree(xvar), mcoeff);
}
void MathStructure::coefficient(const MathStructure &xvar, const Number &pownr, MathStructure &mcoeff) const {
	const MathStructure *mcur = NULL;
	mcoeff.clear();
	for(size_t i = 0; ; i++) {
		if(isAddition()) {
			if(i >= SIZE) break;
			mcur = &CHILD(i);
		} else {
			mcur = this;
		}
		if((*mcur) == xvar) {
			if(pownr.isOne()) {
				if(mcoeff.isZero()) mcoeff.set(1, 1, 0);
				else mcoeff.add(m_one, true);
			}
		} else if(mcur->isPower() && (*mcur)[0] == xvar && (*mcur)[1].isNumber()) {
			if((*mcur)[1].number() == pownr) {
				if(mcoeff.isZero()) mcoeff.set(1, 1, 0);
				else mcoeff.add(m_one, true);
			}
		} else if(mcur->isMultiplication()) {
			bool b = false;
			for(size_t i2 = 0; i2 < mcur->size(); i2++) {
				bool b2 = false;
				if((*mcur)[i2] == xvar) {
					b = true;
					if(pownr.isOne()) b2 = true;
				} else if((*mcur)[i2].isPower() && (*mcur)[i2][0] == xvar && (*mcur)[i2][1].isNumber()) {
					b = true;
					if((*mcur)[i2][1].number() == pownr) b2 = true;
				}
				if(b2) {
					if(mcoeff.isZero()) {
						if(mcur->size() == 1) mcoeff.set(1, 1, 0);
						for(size_t i3 = 0; i3 < mcur->size(); i3++) {
							if(i3 != i2) {
								if(mcoeff.isZero()) mcoeff = (*mcur)[i3];
								else mcoeff.multiply((*mcur)[i3], true);
							}
						}
					} else if(mcur->size() == 1) {
						mcoeff.add(m_one, true);
					} else {
						mcoeff.add(m_zero, true);
						for(size_t i3 = 0; i3 < mcur->size(); i3++) {
							if(i3 != i2) {
								if(mcoeff[mcoeff.size() - 1].isZero()) mcoeff[mcoeff.size() - 1] = (*mcur)[i3];
								else mcoeff[mcoeff.size() - 1].multiply((*mcur)[i3], true);
							}
						}
					}
					break;
				}
			}
			if(!b && pownr.isZero()) {
				if(mcoeff.isZero()) mcoeff = *mcur;
				else mcoeff.add(*mcur, true);
			}
		} else if(pownr.isZero()) {
			if(mcoeff.isZero()) mcoeff = *mcur;
			else mcoeff.add(*mcur, true);
		}
		if(!isAddition()) break;
	}
	mcoeff.evalSort();
}

bool MathStructure::polynomialQuotient(const MathStructure &mnum, const MathStructure &mden, const MathStructure &xvar, MathStructure &mquotient, const EvaluationOptions &eo, bool check_args) {
	mquotient.clear();
	if(CALCULATOR->aborted()) return false;
	if(mden.isZero()) {
		//division by zero
		return false;
	}
	if(mnum.isZero()) {
		mquotient.clear();
		return true;
	}
	if(mden.isNumber() && mnum.isNumber()) {
		mquotient = mnum;
		if(IS_REAL(mden) && IS_REAL(mnum)) {
			mquotient.number() /= mden.number();
		} else {
			mquotient.calculateDivide(mden, eo);
		}
		return true;
	}
	if(mnum == mden) {
		mquotient.set(1, 1, 0);
		return true;
	}
	if(check_args && (!mnum.isRationalPolynomial() || !mden.isRationalPolynomial())) {
		return false;
	}
	
	EvaluationOptions eo2 = eo;
	eo2.keep_zero_units = false;

	Number numdeg = mnum.degree(xvar);
	Number dendeg = mden.degree(xvar);
	MathStructure dencoeff;
	mden.coefficient(xvar, dendeg, dencoeff);
	MathStructure mrem(mnum);
	while(numdeg.isGreaterThanOrEqualTo(dendeg)) {
		if(CALCULATOR->aborted()) return false;
		MathStructure numcoeff;
		mrem.coefficient(xvar, numdeg, numcoeff);
		numdeg -= dendeg;
		if(numcoeff == dencoeff) {
			if(numdeg.isZero()) {
				numcoeff.set(1, 1, 0);
			} else {
				numcoeff = xvar;
				if(!numdeg.isOne()) {
					numcoeff.raise(numdeg);
				}
			}
		} else {
			if(dencoeff.isNumber()) {
				if(numcoeff.isNumber()) {
					numcoeff.number() /= dencoeff.number();
				} else {
					numcoeff.calculateDivide(dencoeff, eo2);
				}
			} else {
				MathStructure mcopy(numcoeff);
				if(!MathStructure::polynomialDivide(mcopy, dencoeff, numcoeff, eo2, false)) {
					return false;
				}
			}
			if(!numdeg.isZero() && !numcoeff.isZero()) {
				if(numcoeff.isOne()) {
					numcoeff = xvar;
					if(!numdeg.isOne()) {
						numcoeff.raise(numdeg);
					}
				} else {
					numcoeff.multiply(xvar, true);
					if(!numdeg.isOne()) {
						numcoeff[numcoeff.size() - 1].raise(numdeg);
					}
					numcoeff.calculateMultiplyLast(eo2);
				}
			}
		}
		if(mquotient.isZero()) mquotient = numcoeff;
		else mquotient.calculateAdd(numcoeff, eo2);
		numcoeff.calculateMultiply(mden, eo2);
		mrem.calculateSubtract(numcoeff, eo2);
		if(mrem.isZero()) break;
		numdeg = mrem.degree(xvar);
	}
	return true;

}

bool remove_zerointerval_multiplier(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.isAddition()) {
		bool b;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(remove_zerointerval_multiplier(mstruct[i], eo)) b = true;
		}
		if(b) {
			mstruct.calculatesub(eo, eo, false);
			return true;
		}
	} else if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isNumber() && !mstruct[i].number().isNonZero()) {
				mstruct.clear(true);
				return true;
			}
		}
	} else if(mstruct.isNumber() && !mstruct.number().isNonZero()) {
		mstruct.clear(true);
		return true;
	}
	return false;
}
bool contains_zerointerval_multiplier(MathStructure &mstruct) {
	if(mstruct.isAddition()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(contains_zerointerval_multiplier(mstruct[i])) return true;
		}
	} else if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isNumber() && !mstruct[i].number().isNonZero()) return true;
		}
	} else if(mstruct.isNumber() && !mstruct.number().isNonZero()) {
		return true;
	}
	return false;
}

bool polynomial_long_division(const MathStructure &mnum, const MathStructure &mden, const MathStructure &xvar_pre, MathStructure &mquotient, MathStructure &mrem, const EvaluationOptions &eo, bool check_args, bool for_newtonraphson) {
	mquotient.clear();
	mrem.clear();
	if(CALCULATOR->aborted()) return false;
	if(mden.isZero()) {
		//division by zero
		mrem.set(mnum);
		return false;
	}
	if(mnum.isZero()) {
		mquotient.clear();
		return true;
	}
	if(mden.isNumber() && mnum.isNumber()) {
		mquotient = mnum;
		if(IS_REAL(mden) && IS_REAL(mnum)) {
			mquotient.number() /= mden.number();
		} else {
			mquotient.calculateDivide(mden, eo);
		}
		return true;
	} else if(mnum.isNumber()) {
		mrem.set(mnum);
		return false;
	}
	if(mnum == mden) {
		mquotient.set(1, 1, 0);
		return true;
	}
	
	mrem.set(mnum);
	
	if(check_args && (!mnum.isRationalPolynomial(true, true) || !mden.isRationalPolynomial(true, true))) {
		return false;
	}
	
	MathStructure xvar(xvar_pre);
	if(xvar.isZero() && !get_first_symbol(mnum, xvar) && !get_first_symbol(mden, xvar)) return false;
	
	EvaluationOptions eo2 = eo;
	eo2.keep_zero_units = false;
	eo2.do_polynomial_division = false;

	Number numdeg = mnum.degree(xvar);
	Number dendeg = mden.degree(xvar);
	MathStructure dencoeff;
	mden.coefficient(xvar, dendeg, dencoeff);
	int i = 0;
	while(numdeg.isGreaterThanOrEqualTo(dendeg)) {
		if(CALCULATOR->aborted()) return false;
		MathStructure numcoeff;
		mrem.coefficient(xvar, numdeg, numcoeff);
		numdeg -= dendeg;
		if(numcoeff == dencoeff) {
			if(numdeg.isZero()) {
				numcoeff.set(1, 1, 0);
			} else {
				numcoeff = xvar;
				if(!numdeg.isOne()) {
					numcoeff.raise(numdeg);
				}
			}
		} else {
			if(dencoeff.isNumber()) {
				if(numcoeff.isNumber()) {
					numcoeff.number() /= dencoeff.number();
				} else {
					numcoeff.calculateDivide(dencoeff, eo2);
				}
			} else {
				if(for_newtonraphson) return false;
				MathStructure mcopy(numcoeff);
				if(!MathStructure::polynomialDivide(mcopy, dencoeff, numcoeff, eo2, check_args)) {
					if(CALCULATOR->aborted()) return false;
					mrem = mcopy;
					break;
				}
			}
			if(!numdeg.isZero() && !numcoeff.isZero()) {
				if(numcoeff.isOne()) {
					numcoeff = xvar;
					if(!numdeg.isOne()) {
						numcoeff.raise(numdeg);
					}
				} else {
					numcoeff.multiply(xvar, true);
					if(!numdeg.isOne()) {
						numcoeff[numcoeff.size() - 1].raise(numdeg);
					}
					numcoeff.calculateMultiplyLast(eo2);
				}
			}
		}
		if(mquotient.isZero()) mquotient = numcoeff;
		else mquotient.calculateAdd(numcoeff, eo2);
		numcoeff.calculateMultiply(mden, eo2);
		mrem.calculateSubtract(numcoeff, eo2);
		if(contains_zerointerval_multiplier(mrem) || contains_zerointerval_multiplier(mquotient)) return false;
		if(mrem.isZero() || (for_newtonraphson && mrem.isNumber())) break;
		numdeg = mrem.degree(xvar);
		i++;
		if(i > 10000) return false;
	}
	return true;

}

void add_symbol(const MathStructure &mpoly, sym_desc_vec &v) {
	sym_desc_vec::const_iterator it = v.begin(), itend = v.end();
	while (it != itend) {
		if(it->sym == mpoly) return;
		++it;
	}
	sym_desc d;
	d.sym = mpoly;
	v.push_back(d);
}

void collect_symbols(const MathStructure &mpoly, sym_desc_vec &v) {
	if(IS_A_SYMBOL(mpoly) || mpoly.isUnit()) {
		add_symbol(mpoly, v);
	} else if(mpoly.isAddition() || mpoly.isMultiplication()) {
		for(size_t i = 0; i < mpoly.size(); i++) collect_symbols(mpoly[i], v);
	} else if(mpoly.isPower()) {
		collect_symbols(mpoly[0], v);
	}
}

void add_symbol(const MathStructure &mpoly, vector<MathStructure> &v) {
	vector<MathStructure>::const_iterator it = v.begin(), itend = v.end();
	while (it != itend) {
		if(*it == mpoly) return;
		++it;
	}
	v.push_back(mpoly);
}

void collect_symbols(const MathStructure &mpoly, vector<MathStructure> &v) {
	if(IS_A_SYMBOL(mpoly)) {
		add_symbol(mpoly, v);
	} else if(mpoly.isAddition() || mpoly.isMultiplication()) {
		for(size_t i = 0; i < mpoly.size(); i++) collect_symbols(mpoly[i], v);
	} else if(mpoly.isPower()) {
		collect_symbols(mpoly[0], v);
	}
}

void get_symbol_stats(const MathStructure &m1, const MathStructure &m2, sym_desc_vec &v) {
	collect_symbols(m1, v);
	collect_symbols(m2, v);
	sym_desc_vec::iterator it = v.begin(), itend = v.end();
	while (it != itend) {
		it->deg_a = m1.degree(it->sym);
		it->deg_b = m2.degree(it->sym);
		if(it->deg_a.isGreaterThan(it->deg_b)) {
			it->max_deg = it->deg_a;
		} else {
			it->max_deg = it->deg_b;
		}
		it->ldeg_a = m1.ldegree(it->sym);
		it->ldeg_b = m2.ldegree(it->sym);
		MathStructure mcoeff;
		m1.lcoefficient(it->sym, mcoeff);
		it->max_lcnops = mcoeff.size();
		m2.lcoefficient(it->sym, mcoeff);
		if(mcoeff.size() > it->max_lcnops) it->max_lcnops = mcoeff.size();
		++it;
	}
	std::sort(v.begin(), v.end());

}

bool MathStructure::gcd(const MathStructure &m1, const MathStructure &m2, MathStructure &mresult, const EvaluationOptions &eo2, MathStructure *ca, MathStructure *cb, bool check_args) {

	EvaluationOptions eo = eo2;
	eo.keep_zero_units = false;

	if(ca) *ca = m1;
	if(cb) *cb = m2;
	mresult.set(1, 1, 0);
	
	if(CALCULATOR->aborted()) return false;

	if(m1.isOne() || m2.isOne()) {
		return true;
	}
	if(m1.isNumber() && m2.isNumber()) {
		mresult = m1;
		if(!mresult.number().gcd(m2.number())) mresult.set(1, 1, 0);
		if(ca || cb) {
			if(mresult.isZero()) {
				if(ca) ca->clear();
				if(cb) cb->clear();
			} else {
				if(ca) {
					*ca = m1;
					ca->number() /= mresult.number();
				}
				if(cb) {
					*cb = m2;
					cb->number() /= mresult.number();
				}
			}
		}
		return true;
	}
	if(m1 == m2) {
		if(ca) ca->set(1, 1, 0);
		if(cb) cb->set(1, 1, 0);
		mresult = m1;
		return true;
	}
	if(m1.isZero()) {
		if(ca) ca->clear();
		if(cb) cb->set(1, 1, 0);
		mresult = m2;
		return true;
	}
	if(m2.isZero()) {
		if(ca) ca->set(1, 1, 0);
		if(cb) cb->clear();
		mresult = m1;
		return true;
	}
	if(check_args && (!m1.isRationalPolynomial() || !m2.isRationalPolynomial())) {
		return false;
	}
	if(m1.isMultiplication()) {
		if(m2.isMultiplication() && m2.size() > m1.size())
			goto factored_2;
factored_1:
		mresult.clear();
		mresult.setType(STRUCT_MULTIPLICATION);
		MathStructure acc_ca;
		acc_ca.setType(STRUCT_MULTIPLICATION);
		MathStructure part_2(m2);
		MathStructure part_ca, part_cb;
		for (size_t i = 0; i < m1.size(); i++) {			
			mresult.addChild(m_zero);
			MathStructure::gcd(m1[i], part_2, mresult[i], eo, &part_ca, &part_cb, false);
			if(CALCULATOR->aborted()) return false;
			acc_ca.addChild(part_ca);
			part_2 = part_cb;
		}
		mresult.calculatesub(eo, eo, false);
		if(ca) {
			*ca = acc_ca;
			ca->calculatesub(eo, eo, false);
		}
		if(cb) *cb = part_2;
		return true;
	} else if(m2.isMultiplication()) {
		if(m1.isMultiplication() && m1.size() > m2.size())
			goto factored_1;
factored_2:
		mresult.clear();
		mresult.setType(STRUCT_MULTIPLICATION);
		MathStructure acc_cb;
		acc_cb.setType(STRUCT_MULTIPLICATION);
		MathStructure part_1(m1);
		MathStructure part_ca, part_cb;
		for(size_t i = 0; i < m2.size(); i++) {
			mresult.addChild(m_zero);
			MathStructure::gcd(part_1, m2[i], mresult[i], eo, &part_ca, &part_cb, false);
			if(CALCULATOR->aborted()) return false;
			acc_cb.addChild(part_cb);
			part_1 = part_ca;
		}
		mresult.calculatesub(eo, eo, false);
		if(ca) *ca = part_1;
		if(cb) {
			*cb = acc_cb;
			cb->calculatesub(eo, eo, false);
		}
		return true;
	}
	if(m1.isPower()) {
		MathStructure p(m1[0]);
		if(m2.isPower()) {
			if(m1[0] == m2[0]) {
				if(m1[1].number().isLessThan(m2[1].number())) {
					if(ca) ca->set(1, 1, 0);
					if(cb) {
						*cb = m2;
						(*cb)[1].number() -= m1[1].number();
						POWER_CLEAN((*cb))
					}
					mresult = m1;
					return true;
				} else {
					if(ca) {
						*ca = m1;
						(*ca)[1].number() -= m2[1].number();
						POWER_CLEAN((*ca))
					}
					if(cb) cb->set(1, 1, 0);
					mresult = m2;
					return true;
				}
			} else {
				MathStructure p_co, pb_co;
				MathStructure p_gcd;
				if(!MathStructure::gcd(m1[0], m2[0], p_gcd, eo, &p_co, &pb_co, false)) return false;
				if(p_gcd.isOne()) {
					return true;
				} else {
					if(m1[1].number().isLessThan(m2[1].number())) {
						mresult = p_gcd;
						mresult.calculateRaise(m1[1], eo);
						mresult.multiply(m_zero, true);
						p_co.calculateRaise(m1[1], eo);
						pb_co.calculateRaise(m2[1], eo);
						p_gcd.raise(m2[1]);
						p_gcd[1].number() -= m1[1].number();
						p_gcd.calculateRaiseExponent(eo);
						p_gcd.calculateMultiply(pb_co, eo);
						bool b = MathStructure::gcd(p_co, p_gcd, mresult[mresult.size() - 1], eo, ca, cb, false);
						mresult.calculateMultiplyLast(eo);
						return b;
					} else {
						mresult = p_gcd;
						mresult.calculateRaise(m2[1], eo);
						mresult.multiply(m_zero, true);
						p_co.calculateRaise(m1[1], eo);
						pb_co.calculateRaise(m2[1], eo);
						p_gcd.raise(m1[1]);
						p_gcd[1].number() -= m2[1].number();
						p_gcd.calculateRaiseExponent(eo);
						p_gcd.calculateMultiply(p_co, eo);
						bool b = MathStructure::gcd(p_gcd, pb_co, mresult[mresult.size() - 1], eo, ca, cb, false);
						mresult.calculateMultiplyLast(eo);
						return b;
					}
				}
			}
		} else {
			if(m1[0] == m2) {
				if(ca) {
					*ca = m1;
					(*ca)[1].number()--;
					POWER_CLEAN((*ca))
				}
				if(cb) cb->set(1, 1, 0);
				mresult = m2;
				return true;
			}
			MathStructure p_co, bpart_co;
			MathStructure p_gcd;
			if(!MathStructure::gcd(m1[0], m2, p_gcd, eo, &p_co, &bpart_co, false)) return false;
			if(p_gcd.isOne()) {
				return true;
			} else {
				mresult = p_gcd;
				mresult.multiply(m_zero, true);
				p_co.calculateRaise(m1[1], eo);
				p_gcd.raise(m1[1]);
				p_gcd[1].number()--;
				p_gcd.calculateRaiseExponent(eo);
				p_gcd.calculateMultiply(p_co, eo);
				bool b = MathStructure::gcd(p_gcd, bpart_co, mresult[mresult.size() - 1], eo, ca, cb, false);
				mresult.calculateMultiplyLast(eo);
				return b;
			}
		}
	} else if(m2.isPower()) {
		if(m2[0] == m1) {
			if(ca) ca->set(1, 1, 0);
			if(cb) {
				*cb = m2;
				(*cb)[1].number()--;
				POWER_CLEAN((*cb))
			}
			mresult = m1;
			return true;
		}
		MathStructure p_co, apart_co;
		MathStructure p_gcd;
		if(!MathStructure::gcd(m1, m2[0], p_gcd, eo, &apart_co, &p_co, false)) return false;
		if(p_gcd.isOne()) {
			return true;
		} else {
			mresult = p_gcd;
			mresult.multiply(m_zero, true);
			p_co.calculateRaise(m2[1], eo);
			p_gcd.raise(m2[1]);
			p_gcd[1].number()--;
			p_gcd.calculateRaiseExponent(eo);
			p_gcd.calculateMultiply(p_co, eo);
			bool b = MathStructure::gcd(apart_co, p_gcd, mresult[mresult.size() - 1], eo, ca, cb, false);
			mresult.calculateMultiplyLast(eo);
			return b;
		}
	}
	if(IS_A_SYMBOL(m1) || m1.isUnit()) {
		MathStructure bex(m2);
		bex.calculateReplace(m1, m_zero, eo);
		if(!bex.isZero()) {
			return true;
		}
	}
	if(IS_A_SYMBOL(m2) || m2.isUnit()) {
		MathStructure aex(m1);
		aex.calculateReplace(m2, m_zero, eo);
		if(!aex.isZero()) {
			return true;
		}
	}

	sym_desc_vec sym_stats;
	get_symbol_stats(m1, m2, sym_stats);
	
	if(sym_stats.empty()) return false;

	size_t var_i = 0;
	const MathStructure &xvar = sym_stats[var_i].sym;

	Number ldeg_a(sym_stats[var_i].ldeg_a);
	Number ldeg_b(sym_stats[var_i].ldeg_b);
	Number min_ldeg;
	if(ldeg_a.isLessThan(ldeg_b)) {
		min_ldeg = ldeg_a;
	} else {
		min_ldeg = ldeg_b;
	}

	if(min_ldeg.isPositive()) {
		MathStructure aex(m1), bex(m2);
		MathStructure common(xvar);
		if(!min_ldeg.isOne()) common.raise(min_ldeg);
		aex.calculateDivide(common, eo);
		bex.calculateDivide(common, eo);
		MathStructure::gcd(aex, bex, mresult, eo, ca, cb, false);
		mresult.calculateMultiply(common, eo);
		return true;
	}
	if(sym_stats[var_i].deg_a.isZero()) {
		if(cb) {
			MathStructure c, mprim;
			int u;
			m2.polynomialUnitContentPrimpart(xvar, u, c, mprim, eo);
			MathStructure::gcd(m1, c, mresult, eo, ca, cb, false);
			if(CALCULATOR->aborted()) {
				if(ca) *ca = m1;
				if(cb) *cb = m2;
				mresult.set(1, 1, 0);
				return false;
			}
			if(u == -1) {
				cb->calculateNegate(eo);
			}
			cb->calculateMultiply(mprim, eo);
		} else {
			MathStructure c;
			m2.polynomialContent(xvar, c, eo);
			MathStructure::gcd(m1, c, mresult, eo, ca, cb, false);
		}
		return true;
	} else if(sym_stats[var_i].deg_b.isZero()) {
		if(ca) {
			MathStructure c, mprim;
			int u;
			m1.polynomialUnitContentPrimpart(xvar, u, c, mprim, eo);
			MathStructure::gcd(c, m2, mresult, eo, ca, cb, false);
			if(CALCULATOR->aborted()) {
				if(ca) *ca = m1;
				if(cb) *cb = m2;
				mresult.set(1, 1, 0);
				return false;
			}
			if(u == -1) {
				ca->calculateNegate(eo);
			}
			ca->calculateMultiply(mprim, eo);
		} else {
			MathStructure c;
			m1.polynomialContent(xvar, c, eo);
			MathStructure::gcd(c, m2, mresult, eo, ca, cb, false);
		}
		return true;
	}

	if(!heur_gcd(m1, m2, mresult, eo, ca, cb, sym_stats, var_i)) {
		sr_gcd(m1, m2, mresult, sym_stats, var_i, eo);
		if(!mresult.isOne()) {
			if(ca) {
				MathStructure::polynomialDivide(m1, mresult, *ca, eo, false);
			}
			if(cb) {
				MathStructure::polynomialDivide(m2, mresult, *cb, eo, false);
			}
		}
		if(CALCULATOR->aborted()) {
			if(ca) *ca = m1;
			if(cb) *cb = m2;
			mresult.set(1, 1, 0);
			return false;
		}
	}
	return true;
}

bool sqrfree_differentiate(const MathStructure &mpoly, const MathStructure &x_var, MathStructure &mdiff, const EvaluationOptions &eo) {
	if(mpoly == x_var) {
		mdiff.set(1, 1, 0);
		return true;
	}
	switch(mpoly.type()) {
		case STRUCT_ADDITION: {
			mdiff.clear();
			mdiff.setType(STRUCT_ADDITION);
			for(size_t i = 0; i < mpoly.size(); i++) {
				mdiff.addChild(m_zero);
				if(!sqrfree_differentiate(mpoly[i], x_var, mdiff[i], eo)) return false;
			}
			mdiff.calculatesub(eo, eo, false);
			break;
		}
		case STRUCT_VARIABLE: {}
		case STRUCT_FUNCTION: {}
		case STRUCT_SYMBOLIC: {}
		case STRUCT_UNIT: {}
		case STRUCT_NUMBER: {
			mdiff.clear();
			break;
		}
		case STRUCT_POWER: {
			if(mpoly[0] == x_var) {
				mdiff = mpoly[1];
				mdiff.multiply(x_var);
				if(!mpoly[1].number().isTwo()) {
					mdiff[1].raise(mpoly[1]);
					mdiff[1][1].number()--;
				}
				mdiff.evalSort(true);
			} else {
				mdiff.clear();
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(mpoly.size() < 1) {
				mdiff.clear();
				break;
			} else if(mpoly.size() < 2) {
				return sqrfree_differentiate(mpoly[0], x_var, mdiff, eo);
			}
			mdiff.clear();
			for(size_t i = 0; i < mpoly.size(); i++) {
				if(mpoly[i] == x_var) {
					if(mpoly.size() == 2) {
						if(i == 0) mdiff = mpoly[1];
						else mdiff = mpoly[0];
					} else {
						mdiff.setType(STRUCT_MULTIPLICATION);
						for(size_t i2 = 0; i2 < mpoly.size(); i2++) {
							if(i2 != i) {
								mdiff.addChild(mpoly[i2]);
							}
						}
					}
					break;
				} else if(mpoly[i].isPower() && mpoly[i][0] == x_var) {
					mdiff = mpoly;
					if(mpoly[i][1].number().isTwo()) {
						mdiff[i].setToChild(1);
					} else {
						mdiff[i][1].number()--;
					}
					if(mdiff[0].isNumber()) {
						mdiff[0].number() *= mpoly[i][1].number();
					} else {
						mdiff.insertChild(mpoly[i][1].number(), 1);
					}
					mdiff.evalSort();
					break;
				}
			}
			break;
		}
		default: {
			return false;
		}	
	}
	return true;
}


bool sqrfree_yun(const MathStructure &a, const MathStructure &xvar, MathStructure &factors, const EvaluationOptions &eo) {
	MathStructure w(a);
	MathStructure z;
	if(!sqrfree_differentiate(a, xvar, z, eo)) {
		return false;
	}
	MathStructure g;
	if(!MathStructure::gcd(w, z, g, eo)) {
		return false;
	}
	if(g.isOne()) {
		factors.addChild(a);
		return true;
	}
	MathStructure y;
	MathStructure tmp;
	do {
		tmp = w;
		if(!MathStructure::polynomialQuotient(tmp, g, xvar, w, eo)) {
			return false;
		}
		if(!MathStructure::polynomialQuotient(z, g, xvar, y, eo)) {
			return false;
		}
		if(!sqrfree_differentiate(w, xvar, tmp, eo)) {
			return false;
		}
		z = y;
		z.calculateSubtract(tmp, eo);
		if(!MathStructure::gcd(w, z, g, eo)) {
			return false;
		}
		factors.addChild(g);
	} while (!z.isZero());
	return true;
}

bool sqrfree_simple(const MathStructure &a, const MathStructure &xvar, MathStructure &factors, const EvaluationOptions &eo) {
	MathStructure w(a);
	size_t i = 0;
	while(true) {
		MathStructure z, zmod;
		if(!sqrfree_differentiate(w, xvar, z, eo)) return false;
		polynomial_smod(z, nr_three, zmod, eo);
		if(z == w) {
			factors.addChild(w);
			break;
		}
		MathStructure mgcd;
		if(!MathStructure::gcd(w, z, mgcd, eo)) return false;
		if(mgcd.isOne() || mgcd == w) {
			factors.addChild(w);
			break;
		}
		MathStructure tmp(w);
		if(!MathStructure::polynomialQuotient(tmp, mgcd, xvar, w, eo)) return false;
		if(!sqrfree_simple(mgcd, xvar, factors, eo)) return false;
		i++;
	}
	return true;
}

void lcmcoeff(const MathStructure &e, const Number &l, Number &nlcm);
void lcmcoeff(const MathStructure &e, const Number &l, Number &nlcm) {
	if(e.isNumber() && e.number().isRational()) {
		nlcm = e.number().denominator();
		nlcm.lcm(l);
	} else if(e.isAddition()) {
		nlcm.set(1, 1, 0);
		for(size_t i = 0; i < e.size(); i++) {
			Number c(nlcm);
			lcmcoeff(e[i], c, nlcm);
		}
		nlcm.lcm(l);
	} else if(e.isMultiplication()) {
		nlcm.set(1, 1, 0);
		for(size_t i = 0; i < e.size(); i++) {
			Number c(nlcm);
			lcmcoeff(e[i], nr_one, c);
			nlcm *= c;
		}
		nlcm.lcm(l);
	} else if(e.isPower()) {
		if(IS_A_SYMBOL(e[0]) || e[0].isUnit()) {
			nlcm = l;
		} else {
			lcmcoeff(e[0], l, nlcm);
			nlcm ^= e[1].number();
		}
	} else {
		nlcm = l;
	}
}

void lcm_of_coefficients_denominators(const MathStructure &e, Number &nlcm) {
	return lcmcoeff(e, nr_one, nlcm);
}

void multiply_lcm(const MathStructure &e, const Number &lcm, MathStructure &mmul, const EvaluationOptions &eo);
void multiply_lcm(const MathStructure &e, const Number &lcm, MathStructure &mmul, const EvaluationOptions &eo) {
	if(e.isMultiplication()) {
		Number lcm_accum(1, 1);
		mmul.clear();
		for(size_t i = 0; i < e.size(); i++) {
			Number op_lcm;
			lcmcoeff(e[i], nr_one, op_lcm);
			if(mmul.isZero()) {
				multiply_lcm(e[i], op_lcm, mmul, eo);
				if(mmul.isOne()) mmul.clear();
			} else {
				mmul.multiply(m_one, true);
				multiply_lcm(e[i], op_lcm, mmul[mmul.size() - 1], eo);
				if(mmul[mmul.size() - 1].isOne()) {
					mmul.delChild(i + 1);
					if(mmul.size() == 1) mmul.setToChild(1);
				}
			}
			lcm_accum *= op_lcm;
		}
		Number lcm2(lcm);
		lcm2 /= lcm_accum;
		if(mmul.isZero()) {
			mmul = lcm2;
		} else if(!lcm2.isOne()) {
			if(mmul.size() > 0 && mmul[0].isNumber()) {
				mmul[0].number() *= lcm2;
			} else {				
				mmul.multiply(lcm2, true);
			}
		}
		mmul.evalSort();
	} else if(e.isAddition()) {
		mmul.clear();
		for (size_t i = 0; i < e.size(); i++) {
			if(mmul.isZero()) {
				multiply_lcm(e[i], lcm, mmul, eo);
			} else {
				mmul.add(m_zero, true);
				multiply_lcm(e[i], lcm, mmul[mmul.size() - 1], eo);
			}
		}
		mmul.evalSort();
	} else if(e.isPower()) {
		if(IS_A_SYMBOL(e[0]) || e[0].isUnit()) {
			mmul = e;
			if(!lcm.isOne()) {
				mmul *= lcm;
				mmul.evalSort();
			}
		} else {
			mmul = e;
			Number lcm_exp = e[1].number();
			lcm_exp.recip();
			multiply_lcm(e[0], lcm ^ lcm_exp, mmul[0], eo);
			if(mmul[0] != e[0]) {
				mmul.calculatesub(eo, eo, false);
			}
		}
	} else if(e.isNumber()) {
		mmul = e;
		mmul.number() *= lcm;	
	} else if(IS_A_SYMBOL(e) || e.isUnit()) {
		mmul = e;
		if(!lcm.isOne()) {
			mmul *= lcm;
			mmul.evalSort();
		}
	} else {
		mmul = e;
		if(!lcm.isOne()) {
			mmul.calculateMultiply(lcm, eo);
			mmul.evalSort();
		}
	}
}

//from GiNaC
bool sqrfree(MathStructure &mpoly, const EvaluationOptions &eo) {
	vector<MathStructure> symbols;
	collect_symbols(mpoly, symbols);
	return sqrfree(mpoly, symbols, eo);
}
bool sqrfree(MathStructure &mpoly, const vector<MathStructure> &symbols, const EvaluationOptions &eo) {

	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = true;
	eo2.warn_about_denominators_assumed_nonzero = false;
	eo2.reduce_divisions = true;
	eo2.keep_zero_units = false;
	eo2.do_polynomial_division = false;
	eo2.sync_units = false;
	eo2.expand = true;
	eo2.calculate_functions = false;
	eo2.protected_function = CALCULATOR->f_signum;

	if(mpoly.size() == 0) {
		return true;
	}
	if(symbols.empty()) return true;
	
	size_t symbol_index = 0;
	if(symbols.size() > 1) {
		for(size_t i = 1; i < symbols.size(); i++) {
			if(mpoly.degree(symbols[symbol_index]).isGreaterThan(mpoly.degree(symbols[i]))) symbol_index = i;
		}
	}

	const MathStructure &xvar = symbols[symbol_index];

	Number nlcm;
	lcm_of_coefficients_denominators(mpoly, nlcm);
	
	MathStructure tmp;
	multiply_lcm(mpoly, nlcm, tmp, eo2);

	MathStructure factors;
	factors.clearVector();
	if(!sqrfree_yun(tmp, xvar, factors, eo2)) {
		factors.clearVector();
		factors.addChild(tmp);
	}

	vector<MathStructure> newsymbols;
	for(size_t i = 0; i < symbols.size(); i++) {
		if(i != symbol_index) newsymbols.push_back(symbols[i]);
	}

	if(newsymbols.size() > 0) {
		for(size_t i = 0; i < factors.size(); i++) {
			if(!sqrfree(factors[i], newsymbols, eo)) return false;
		}
	}

	mpoly.set(1, 1, 0);
	for(size_t i = 0; i < factors.size(); i++) {
		if(CALCULATOR->aborted()) return false;
		if(!factors[i].isOne()) {
			if(mpoly.isOne()) {
				mpoly = factors[i];
				if(i != 0) mpoly.raise(MathStructure((long int) i + 1, 1L, 0L));
			} else {
				mpoly.multiply(factors[i], true);
				mpoly[mpoly.size() - 1].raise(MathStructure((long int) i + 1, 1L, 0L));
			}
		}
	}
	
	if(CALCULATOR->aborted()) return false;
	if(mpoly.isZero()) {
		CALCULATOR->error(true, "mpoly is zero: %s. %s", tmp.print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}
	MathStructure mquo;
	MathStructure mpoly_expand(mpoly);
	EvaluationOptions eo3 = eo;
	eo3.expand = true;
	mpoly_expand.calculatesub(eo3, eo3);

	MathStructure::polynomialQuotient(tmp, mpoly_expand, xvar, mquo, eo2);

	if(CALCULATOR->aborted()) return false;
	if(mquo.isZero()) {
		//CALCULATOR->error(true, "quo is zero: %s. %s", tmp.print().c_str(), _("This is a bug. Please report it."), NULL);
		return false;
	}
	if(newsymbols.size() > 0) {
		if(!sqrfree(mquo, newsymbols, eo)) return false;
	}
	if(!mquo.isOne()) {
		mpoly.multiply(mquo, true);
	}
	if(!nlcm.isOne()) {
		nlcm.recip();
		mpoly.multiply(nlcm, true);
	}

	eo3.expand = false;
	mpoly.calculatesub(eo3, eo3, false);

	return true;

}

bool MathStructure::integerFactorize() {
	if(!isNumber()) return false;
	vector<Number> factors;
	if(!o_number.factorize(factors)) return false;
	if(factors.size() <= 1) return true;
	clear(true);
	bool b_pow = false;
	Number *lastnr = NULL;
	for(size_t i = 0; i < factors.size(); i++) {
		if(lastnr && factors[i] == *lastnr) {
			if(!b_pow) {
				LAST.raise(m_one);
				b_pow = true;
			}
			LAST[1].number()++;
		} else {
			APPEND(factors[i]);
			b_pow = false;
		}
		lastnr = &factors[i];
	}
	m_type = STRUCT_MULTIPLICATION;
	return true;
}
size_t count_powers(const MathStructure &mstruct) {
	if(mstruct.isPower()) {
		if(mstruct[1].isInteger()) {
			bool overflow = false;
			int c = mstruct.number().intValue(&overflow) - 1;
			if(overflow) return 0;
			if(c < 0) return -c;
			return c;
		}
	}
	size_t c = 0;
	for(size_t i = 0; i < mstruct.size(); i++) {
		c += count_powers(mstruct[i]);
	}
	return c;
}
bool MathStructure::factorize(const EvaluationOptions &eo_pre, bool unfactorize, int term_combination_levels, int max_msecs, bool only_integers, int recursive, struct timeval *endtime_p, const MathStructure &force_factorization) {
	if(CALCULATOR->aborted()) return false;
	struct timeval endtime;
	if(max_msecs > 0 && !endtime_p) {
#ifndef CLOCK_MONOTONIC
		gettimeofday(&endtime, NULL);
#else
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		endtime.tv_sec = ts.tv_sec;
		endtime.tv_usec = ts.tv_nsec / 1000;
#endif
		endtime.tv_sec += max_msecs / 1000;
		long int usecs = endtime.tv_usec + (long int) (max_msecs % 1000) * 1000;
		if(usecs >= 1000000) {
			usecs -= 1000000;
			endtime.tv_sec++;
		}
		endtime.tv_usec = usecs;
		max_msecs = 0;
		endtime_p = &endtime;
	}

	EvaluationOptions eo = eo_pre;
	eo.sync_units = false;
	eo.structuring = STRUCTURING_NONE;
	if(unfactorize) {
		unformat();
		EvaluationOptions eo2 = eo;
		eo2.expand = true;
		eo2.combine_divisions = false;
		eo2.sync_units = false;
		calculatesub(eo2, eo2);
		do_simplification(*this, eo, true, false, true);
	}

	MathStructure mden, mnum;
	evalSort(true);
	if(term_combination_levels >= -1 && isAddition() && isRationalPolynomial()) {
		MathStructure msqrfree(*this);
		eo.protected_function = CALCULATOR->f_signum;
		if(sqrfree(msqrfree, eo)) {
			if(!equals(msqrfree) && (!msqrfree.isMultiplication() || msqrfree.size() != 2 || (!(msqrfree[0].isNumber() && msqrfree[1].isAddition()) && !(msqrfree[1].isNumber() && msqrfree[0].isAddition())))) {
				MathStructure mcopy(msqrfree);
				EvaluationOptions eo2 = eo;
				eo2.expand = true;
				eo2.calculate_functions = false;
				CALCULATOR->beginTemporaryStopMessages();
				mcopy.calculatesub(eo2, eo2);
				CALCULATOR->endTemporaryStopMessages();
				if(!equals(mcopy)) {
					eo.protected_function = eo_pre.protected_function;
					if(CALCULATOR->aborted()) return false;
					CALCULATOR->error(true, "factorized result is wrong: %s. %s", msqrfree.print().c_str(), _("This is a bug. Please report it."), NULL);
				} else {
					eo.protected_function = eo_pre.protected_function;
					set(msqrfree);
					if(!isAddition()) {
						if(isMultiplication()) flattenMultiplication(*this);
						if(isMultiplication() && SIZE >= 2 && CHILD(0).isNumber()) {
							for(size_t i = 1; i < SIZE; i++) {
								if(CHILD(i).isNumber()) {
									CHILD(i).number() *= CHILD(0).number();
									CHILD(0).set(CHILD(i));
									delChild(i);
								} else if(CHILD(i).isPower() && CHILD(i)[0].isMultiplication() && CHILD(i)[0].size() >= 2 && CHILD(i)[0][0].isNumber() && CHILD(i)[0][0].number().isRational() && !CHILD(i)[0][0].number().isInteger() && CHILD(i)[1].isInteger()) {
									CHILD(i)[0][0].number().raise(CHILD(i)[1].number());
									CHILD(0).number().multiply(CHILD(i)[0][0].number());
									CHILD(i)[0].delChild(1);
									if(CHILD(i)[0].size() == 1) CHILD(i)[0].setToChild(1, true);
								}
							}
							if(SIZE > 1 && CHILD(0).isOne()) {
								ERASE(0);
							}
							if(SIZE == 1) SET_CHILD_MAP(0);
						}
						if(isMultiplication() && SIZE >= 2 && CHILD(0).isNumber() && CHILD(0).number().isRational() && !CHILD(0).number().isInteger()) {
							Number den = CHILD(0).number().denominator();
							for(size_t i = 1; i < SIZE; i++) {
								if(CHILD(i).isAddition()) {
									bool b = true;
									for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
										if(CHILD(i)[i2].isNumber()) {
											if(!CHILD(i)[i2].number().isIntegerDivisible(den)) {b = false; break;}
										} else if(CHILD(i)[i2].isMultiplication() && CHILD(i)[i2][0].isNumber()) {
											if(!CHILD(i)[i2][0].number().isIntegerDivisible(den)) {b = false; break;}
										} else {
											b = false;
											break;
										}
									}
									if(b) {
										for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
											if(CHILD(i)[i2].isNumber()) {
												CHILD(i)[i2].number().divide(den);
											} else if(CHILD(i)[i2].isMultiplication()) {
												CHILD(i)[i2][0].number().divide(den);
												if(CHILD(i)[i2][0].isOne() && CHILD(i)[i2].size() > 1) {
													CHILD(i)[i2].delChild(1);
													if(CHILD(i)[i2].size() == 1) {
														CHILD(i)[i2].setToChild(1, true);
													}
												}
											}
										}
										CHILD(0).set(CHILD(0).number().numerator(), true);
										if(SIZE > 1 && CHILD(0).isOne()) {
											ERASE(0);
										}
										if(SIZE == 1) SET_CHILD_MAP(0);
										break;
									}
								}
							}
						}
						if(isMultiplication()) {
							for(size_t i = 0; i < SIZE; i++) {
								if(CHILD(i).isPower() && CHILD(i)[1].isInteger()) {
									if(CHILD(i)[0].isAddition()) {
										bool b = true;
										for(size_t i2 = 0; i2 < CHILD(i)[0].size(); i2++) {
											if((!CHILD(i)[0][i2].isNumber() || !CHILD(i)[0][i2].number().isNegative()) && (!CHILD(i)[0][i2].isMultiplication() || CHILD(i)[0][i2].size() < 2 || !CHILD(i)[0][i2][0].isNumber() || !CHILD(i)[0][i2][0].number().isNegative())) {
												b = false;
												break;
											}
										}
										if(b) {
											for(size_t i2 = 0; i2 < CHILD(i)[0].size(); i2++) {
												if(CHILD(i)[0][i2].isNumber()) {
													CHILD(i)[0][i2].number().negate();
												} else {
													CHILD(i)[0][i2][0].number().negate();
													if(CHILD(i)[0][i2][0].isOne() && CHILD(i)[0][i2].size() > 1) {
														CHILD(i)[0][i2].delChild(1);
														if(CHILD(i)[0][i2].size() == 1) {
															CHILD(i)[0][i2].setToChild(1, true);
														}
													}
												}
											}
											if(CHILD(i)[1].number().isOdd()) {
												if(CHILD(0).isNumber()) CHILD(0).number().negate();
												else {
													PREPEND(MathStructure(-1, 1, 0));
													i++;
												}
											}
										}
									} else if(CHILD(i)[0].isMultiplication() && CHILD(i)[0].size() >= 2 && CHILD(i)[0][0].isNumber() && CHILD(i)[0][0].number().isNegative()) {
										CHILD(i)[0][0].number().negate();
										if(CHILD(i)[0][0].isOne() && CHILD(i)[0].size() > 1) {
											CHILD(i)[0].delChild(1);
											if(CHILD(i)[0].size() == 1) {
												CHILD(i)[0].setToChild(1, true);
											}
										}
										if(CHILD(i)[1].number().isOdd()) {
											if(CHILD(0).isNumber()) CHILD(0).number().negate();
											else {
												PREPEND(MathStructure(-1, 1, 0));
												i++;
											}
										}
									}
								} else if(CHILD(i).isAddition()) {
									bool b = true;
									for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
										if((!CHILD(i)[i2].isNumber() || !CHILD(i)[i2].number().isNegative()) && (!CHILD(i)[i2].isMultiplication() || CHILD(i)[i2].size() < 2 || !CHILD(i)[i2][0].isNumber() || !CHILD(i)[i2][0].number().isNegative())) {
											b = false;
											break;
										}
									}
									if(b) {
										for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
											if(CHILD(i)[i2].isNumber()) {
												CHILD(i)[i2].number().negate();
											} else {
												CHILD(i)[i2][0].number().negate();
												if(CHILD(i)[i2][0].isOne() && CHILD(i)[i2].size() > 1) {
													CHILD(i)[i2].delChild(1);
													if(CHILD(i)[i2].size() == 1) {
														CHILD(i)[i2].setToChild(1, true);
													}
												}
											}
										}
										if(CHILD(0).isNumber()) CHILD(0).number().negate();
										else {
											PREPEND(MathStructure(-1, 1, 0));
											i++;
										}
									}
								}
							}
							if(SIZE > 1 && CHILD(0).isOne()) {
								ERASE(0);
							}
							if(SIZE == 1) SET_CHILD_MAP(0);
						}
						if(isPower() && CHILD(1).isInteger()) {
							if(CHILD(0).isAddition()) {
								bool b = true;
								for(size_t i2 = 0; i2 < CHILD(0).size(); i2++) {
									if((!CHILD(0)[i2].isNumber() || !CHILD(0)[i2].number().isNegative()) && (!CHILD(0)[i2].isMultiplication() || CHILD(0)[i2].size() < 2 || !CHILD(0)[i2][0].isNumber() || !CHILD(0)[i2][0].number().isNegative())) {
										b = false;
										break;
									}
								}
								if(b) {
									for(size_t i2 = 0; i2 < CHILD(0).size(); i2++) {
										if(CHILD(0)[i2].isNumber()) {
											CHILD(0)[i2].number().negate();
										} else {
											CHILD(0)[i2][0].number().negate();
											if(CHILD(0)[i2][0].isOne() && CHILD(0)[i2].size() > 1) {
												CHILD(0)[i2].delChild(1);
												if(CHILD(0)[i2].size() == 1) {
													CHILD(0)[i2].setToChild(1, true);
												}
											}
										}
									}
									if(CHILD(1).number().isOdd()) {
										multiply(MathStructure(-1, 1, 0));
										CHILD_TO_FRONT(1)
									}
								}
							} else if(CHILD(0).isMultiplication() && CHILD(0).size() >= 2 && CHILD(0)[0].isNumber() && CHILD(0)[0].number().isNegative()) {
								CHILD(0)[0].number().negate();
								if(CHILD(0)[0].isOne() && CHILD(0).size() > 1) {
									CHILD(0).delChild(1);
									if(CHILD(0).size() == 1) {
										CHILD(0).setToChild(1, true);
									}
								}
								if(CHILD(1).number().isOdd()) {
									multiply(MathStructure(-1, 1, 0));
									CHILD_TO_FRONT(1)
								}
							}
						}
					}
					evalSort(true);
					factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
					return true;
				}
			}
		}
		eo.protected_function = eo_pre.protected_function;
	}
	switch(type()) {
		case STRUCT_ADDITION: {
			if(CALCULATOR->aborted()) return false;
			if(term_combination_levels >= -1) {
				/*if(SIZE >= 2 && LAST.isInteger() && LAST.number().isLessThan(1000) && LAST.number().isGreaterThan(-1000) && ((CHILD(0).isPower() && CHILD(0)[1].isInteger() && CHILD(0)[1].number().isGreaterThan(2)) || (CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isInteger() && CHILD(0)[0].number().isLessThan(1000) && CHILD(0)[0].number().isGreaterThan(-1000) && CHILD(0)[1].isPower() && CHILD(0)[1][1].isInteger() && CHILD(0)[1][1].number().isGreaterThan(2)))) {
					bool b = true;
					MathStructure x_var;
					vector<Number> nfacs;
					vector<Number> npows;
					Number n0(LAST.number());
					if(CHILD(0).isPower()) {
						npows.push_back(CHILD(0)[1].number());
						nfacs.push_back(nr_one);
						x_var = CHILD(0)[0];
					} else {
						npows.push_back(CHILD(0)[1][1].number());
						nfacs.push_back(CHILD(0)[0].number());
						x_var = CHILD(0)[1][0];
					}
					for(size_t i = 1; i < SIZE - 1; i++) {
						if(CHILD(i) == x_var) {
							npows.push_back(nr_one);
							nfacs.push_back(nr_one);
						} else if(CHILD(i).isPower() && CHILD(i)[0] == x_var && CHILD(i)[1].isInteger() && CHILD(i)[1].number().isPositive()) {
							npows.push_back(CHILD(i)[1].number());
							nfacs.push_back(nr_one);
						} else if(CHILD(i).isMultiplication() && CHILD(i).size() == 2 && CHILD(i)[0].isInteger() && ((CHILD(i)[1].isPower() && CHILD(i)[1][0] == x_var && CHILD(i)[1][1].isInteger() && CHILD(i)[1][1].number().isPositive()) || CHILD(i)[1] == x_var)) {
							nfacs.push_back(CHILD(i)[0].number());
							if(CHILD(i)[1].isPower()) npows.push_back(CHILD(i)[1][1].number());
							else npows.push_back(nr_one);
						} else {
							b = false;
							break;
						}
					}
					if(b) {
						Number nt;
						int n0i = n0.intValue();
						if(n0i < 0) n0i = -n0i;
						int nli = nfacs[0].intValue();
						if(nli < 0) nli = -nli;
						for(int i = 1; i <= n0i; i++) {
							if(n0i % i == 0) {
								for(int il = 1; il <= nli; il++) {
									if(nli % il == 0 && (!only_integers || i % il == 0)) {
										Number nr(i, il);
										Number nrn;
										nt = n0;
										for(size_t i2 = 0; i2 < nfacs.size(); i2++) {
											nrn = nr;
											nrn ^= npows[i2];
											nrn *= nfacs[i2];
											nt += nrn;
										}
										if(nt.isZero()) {
											MathStructure mden(x_var), mquo, mrem;
											mden += nr;
											if(polynomial_long_division(*this, mden, x_var, mquo, mrem, eo) && mrem.isZero()) {
												set(mden);
												multiply(mquo);
												return true;
											}
											nr.negate();
											mden = x_var;
											mden += nr;
											if(polynomial_long_division(*this, mden, x_var, mquo, mrem, eo) && mrem.isZero()) {
												set(mden);
												multiply(mquo);
												return true;
											}
										} else {
											nr.negate();
											nt = n0;
											for(size_t i2 = 0; i2 < nfacs.size(); i2++) {
												nrn = nr;
												nrn ^= npows[i2];
												nrn *= nfacs[i2];
												nt += nrn;
											}
											if(nt.isZero()) {
												MathStructure mden(x_var), mquo, mrem;
												mden += nr;
												if(polynomial_long_division(*this, mden, x_var, mquo, mrem, eo) && mrem.isZero()) {
													set(mden);
													multiply(mquo);
													return true;
												}
												nr.negate();
												mden = x_var;
												mden += nr;
												if(polynomial_long_division(*this, mden, x_var, mquo, mrem, eo) && mrem.isZero()) {
													set(mden);
													multiply(mquo);
													return true;
												}
											}
										}
									}
								}
							}
						}
					}
				}*/

				if(SIZE <= 3 && SIZE > 1) {
					MathStructure *xvar = NULL;
					Number nr2(1, 1);
					if(CHILD(0).isPower() && CHILD(0)[0].size() == 0 && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isTwo()) {
						xvar = &CHILD(0)[0];
					} else if(CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isNumber()) {
						if(CHILD(0)[1].isPower()) {
							if(CHILD(0)[1][0].size() == 0 && CHILD(0)[1][1].isNumber() && CHILD(0)[1][1].number().isTwo()) {
								xvar = &CHILD(0)[1][0];
								nr2.set(CHILD(0)[0].number());
							}
						}
					}
					if(xvar) {
						bool factorable = false;
						Number nr1, nr0;
						if(SIZE == 2 && CHILD(1).isNumber()) {
							factorable = true;
							nr0 = CHILD(1).number();
						} else if(SIZE == 3 && CHILD(2).isNumber()) {
							nr0 = CHILD(2).number();
							if(CHILD(1).isMultiplication()) {
								if(CHILD(1).size() == 2 && CHILD(1)[0].isNumber() && xvar->equals(CHILD(1)[1])) {
									nr1 = CHILD(1)[0].number();
									factorable = true;
								}
							} else if(xvar->equals(CHILD(1))) {
								nr1.set(1, 1, 0);
								factorable = true;
							}
						}
						if(factorable) {
							Number nr4ac(4, 1, 0);
							nr4ac *= nr2;
							nr4ac *= nr0;
							Number nr2a(2, 1, 0);
							nr2a *= nr2;
							Number sqrtb24ac(nr1);
							sqrtb24ac.raise(nr_two);
							sqrtb24ac -= nr4ac;
							if(sqrtb24ac.isNegative()) factorable = false;
							MathStructure mstructb24(sqrtb24ac);
							if(factorable) {
								if(!only_integers) {
									if(eo.approximation == APPROXIMATION_EXACT && !sqrtb24ac.isApproximate()) {
										sqrtb24ac.raise(nr_half);
										if(sqrtb24ac.isApproximate()) {
											mstructb24.raise(nr_half);
										} else {
											mstructb24.set(sqrtb24ac);
										}
									} else {
										mstructb24.number().raise(nr_half);
									}
								} else {
									mstructb24.number().raise(nr_half);
									if((!sqrtb24ac.isApproximate() && mstructb24.number().isApproximate()) || (sqrtb24ac.isInteger() && !mstructb24.number().isInteger())) {
										factorable = false;
									}
								}
							}
							if(factorable) {
								MathStructure m1(nr1), m2(nr1);
								Number mul1(1, 1), mul2(1, 1);
								if(mstructb24.isNumber()) {
									m1.number() += mstructb24.number();
									m1.number() /= nr2a;
									if(m1.number().isRational() && !m1.number().isInteger()) {
										mul1 = m1.number().denominator();
										m1.number() *= mul1;
									}
									m2.number() -= mstructb24.number();
									m2.number() /= nr2a;
									if(m2.number().isRational() && !m2.number().isInteger()) {
										mul2 = m2.number().denominator();
										m2.number() *= mul2;
									}
								} else {
									m1.calculateAdd(mstructb24, eo);
									m1.calculateDivide(nr2a, eo);
									if(m1.isNumber()) {
										if(m1.number().isRational() && !m1.number().isInteger()) {
											mul1 = m1.number().denominator();
											m1.number() *= mul1;
										}
									} else {
										bool bint = false, bfrac = false;
										idm1(m1, bfrac, bint);
										if(bfrac) {
											idm2(m1, bfrac, bint, mul1);
											idm3(m1, mul1, true);
										}
									}
									m2.calculateSubtract(mstructb24, eo);
									m2.calculateDivide(nr2a, eo);
									if(m2.isNumber()) {
										if(m2.number().isRational() && !m2.number().isInteger()) {
											mul2 = m2.number().denominator();
											m2.number() *= mul2;
										}
									} else {
										bool bint = false, bfrac = false;
										idm1(m2, bfrac, bint);
										if(bfrac) {
											idm2(m2, bfrac, bint, mul2);
											idm3(m2, mul2, true);
										}
									}
								}
								nr2 /= mul1;
								nr2 /= mul2;
								if(m1 == m2 && mul1 == mul2) {
									MathStructure xvar2(*xvar);
									if(!mul1.isOne()) xvar2 *= mul1;
									set(m1);
									add(xvar2, true);
									raise(MathStructure(2, 1, 0));
									if(!nr2.isOne()) {
										multiply(nr2);
									}
								} else {				
									m1.add(*xvar, true);
									if(!mul1.isOne()) m1[m1.size() - 1] *= mul1;
									m2.add(*xvar, true);
									if(!mul2.isOne()) m2[m2.size() - 1] *= mul2;
									clear(true);
									m_type = STRUCT_MULTIPLICATION;
									if(!nr2.isOne()) {
										APPEND_NEW(nr2);
									}
									APPEND(m1);
									APPEND(m2);
								}
								EvaluationOptions eo2 = eo;
								eo2.expand = false;
								calculatesub(eo2, eo2, false);
								evalSort(true);
								return true;
							}
						}
					}
				}

				MathStructure *factor_mstruct = new MathStructure(1, 1, 0);
				MathStructure mnew;
				if(factorize_find_multiplier(*this, mnew, *factor_mstruct)) {
					mnew.factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
					factor_mstruct->factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
					clear(true);
					m_type = STRUCT_MULTIPLICATION;
					APPEND_REF(factor_mstruct);
					APPEND(mnew);
					EvaluationOptions eo2 = eo;
					eo2.expand = false;
					calculatesub(eo2, eo2, false);
					factor_mstruct->unref();
					evalSort(true);
					return true;
				}
				factor_mstruct->unref();
				if(SIZE > 1 && CHILD(SIZE - 1).isNumber() && CHILD(SIZE - 1).number().isInteger()) {
					MathStructure *xvar = NULL;
					Number qnr(1, 1);
					int degree = 1;
					bool overflow = false;
					int qcof = 1;
					if(CHILD(0).isPower() && CHILD(0)[0].size() == 0 && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isInteger() && CHILD(0)[1].number().isPositive()) {
						xvar = &CHILD(0)[0];
						degree = CHILD(0)[1].number().intValue(&overflow);
					} else if(CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isNumber() && CHILD(0)[0].number().isInteger()) {
						if(CHILD(0)[1].isPower()) {
							if(CHILD(0)[1][0].size() == 0 && CHILD(0)[1][1].isNumber() && CHILD(0)[1][1].number().isInteger() && CHILD(0)[1][1].number().isPositive()) {
								xvar = &CHILD(0)[1][0];
								qcof = CHILD(0)[0].number().intValue(&overflow);
								if(!overflow) {
									if(qcof < 0) qcof = -qcof;
									degree = CHILD(0)[1][1].number().intValue(&overflow);
								}
							}
						}
					}
					int pcof = 1;
					if(!overflow) {
						pcof = CHILD(SIZE - 1).number().intValue(&overflow);
						if(pcof < 0) pcof = -pcof;
					}
					if(xvar && !overflow && degree <= 1000 && degree > 2) {
						bool b = true;
						for(size_t i = 1; b && i < SIZE - 1; i++) {
							switch(CHILD(i).type()) {
								case STRUCT_NUMBER: {
									b = false;
									break;
								}
								case STRUCT_POWER: {
									if(!CHILD(i)[1].isNumber() || !xvar->equals(CHILD(i)[0]) || !CHILD(i)[1].number().isInteger() || !CHILD(i)[1].number().isPositive()) {
										b = false;
									}
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(!(CHILD(i).size() == 2) || !CHILD(i)[0].isNumber()) {
										b = false;
									} else if(CHILD(i)[1].isPower()) {
										if(!CHILD(i)[1][1].isNumber() || !xvar->equals(CHILD(i)[1][0]) || !CHILD(i)[1][1].number().isInteger() || !CHILD(i)[1][1].number().isPositive()) {
											b = false;
										}
									} else if(!xvar->equals(CHILD(i)[1])) {
										b = false;
									}
									break;
								}
								default: {
									if(!xvar->equals(CHILD(i))) {
										b = false;
									}
								}
							}
						}
						if(b) {
							vector<Number> factors;
							factors.resize(degree + 1, Number());
							factors[0] = CHILD(SIZE - 1).number();
							vector<int> ps;
							vector<int> qs;
							vector<Number> zeroes;
							int curdeg = 1, prevdeg = 0;
							for(size_t i = 0; b && i < SIZE - 1; i++) {
								switch(CHILD(i).type()) {
									case STRUCT_POWER: {
										curdeg = CHILD(i)[1].number().intValue(&overflow);
										if(curdeg == prevdeg || curdeg > degree || (prevdeg > 0 && curdeg > prevdeg) || overflow) {
											b = false;
										} else {
											factors[curdeg].set(1, 1, 0);
										}
										break;
									}
									case STRUCT_MULTIPLICATION: {
										if(CHILD(i)[1].isPower()) {
											curdeg = CHILD(i)[1][1].number().intValue(&overflow);
										} else {
											curdeg = 1;
										}
										if(curdeg == prevdeg || curdeg > degree || (prevdeg > 0 && curdeg > prevdeg) || overflow) {
											b = false;
										} else {
											factors[curdeg] = CHILD(i)[0].number();
										}
										break;
									}
									default: {
										curdeg = 1;
										factors[curdeg].set(1, 1, 0);
									}
								}
								prevdeg = curdeg;
							}
							while(b && degree > 2) {
								for(int i = 1; i <= 1000; i++) {
									if(i > pcof) break;
									if(pcof % i == 0) ps.push_back(i);
								}
								for(int i = 1; i <= 1000; i++) {
									if(i > qcof) break;
									if(qcof % i == 0) qs.push_back(i);
								}
								Number itest;
								int i2;
								size_t pi = 0, qi = 0;
								Number nrtest(ps[0], qs[0], 0);
								while(true) {
									itest.clear(); i2 = degree;
									while(true) {
										itest += factors[i2];
										if(i2 == 0) break;
										itest *= nrtest;
										i2--;
									}
									if(itest.isZero()) {
										break;
									}
									if(nrtest.isPositive()) {
										nrtest.negate();
									} else {
										qi++;
										if(qi == qs.size()) {
											qi = 0;
											pi++;
											if(pi == ps.size()) {
												break;
											}
										}
										nrtest.set(ps[pi], qs[qi], 0);
									}
								}
								if(itest.isZero()) {
									itest.clear(); i2 = degree;
									Number ntmp(factors[i2]);
									for(; i2 > 0; i2--) {
										itest += ntmp;
										ntmp = factors[i2 - 1];
										factors[i2 - 1] = itest;
										itest *= nrtest;
									}
									degree--;
									nrtest.negate();
									zeroes.push_back(nrtest);
									if(degree == 2) {
										break;
									}
									qcof = factors[degree].intValue(&overflow);
									if(!overflow) {
										if(qcof < 0) qcof = -qcof;
										pcof = factors[0].intValue(&overflow);
										if(!overflow) {
											if(pcof < 0) pcof = -pcof;
										}
									}
									if(overflow) {
										break;
									}
								} else {
									break;
								}
								ps.clear();
								qs.clear();
							}
							if(zeroes.size() > 0) {
								MathStructure mleft;
								MathStructure mtmp;
								MathStructure *mcur;
								for(int i = degree; i >= 0; i--) {
									if(!factors[i].isZero()) {
										if(mleft.isZero()) {
											mcur = &mleft;
										} else {
											mleft.add(m_zero, true);
											mcur = &mleft[mleft.size() - 1];
										}
										if(i > 1) {
											if(!factors[i].isOne()) {
												mcur->multiply(*xvar);
												(*mcur)[0].set(factors[i]);
												mcur = &(*mcur)[1];
											} else {
												mcur->set(*xvar);
											}
											mtmp.set(i, 1, 0);
											mcur->raise(mtmp);
										} else if(i == 1) {
											if(!factors[i].isOne()) {
												mcur->multiply(*xvar);
												(*mcur)[0].set(factors[i]);
											} else {
												mcur->set(*xvar);
											}
										} else {
											mcur->set(factors[i]);
										}
									}
								}
								mleft.factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
								vector<long int> powers;
								vector<size_t> powers_i;
								int dupsfound = 0;
								for(size_t i = 0; i < zeroes.size() - 1; i++) {
									while(i + 1 < zeroes.size() && zeroes[i] == zeroes[i + 1]) {
										dupsfound++;
										zeroes.erase(zeroes.begin() + (i + 1));
									}
									if(dupsfound > 0) {
										powers_i.push_back(i);
										powers.push_back(dupsfound + 1);
										dupsfound = 0;
									}
								}
								MathStructure xvar2(*xvar);
								Number *nrmul;
								if(mleft.isMultiplication()) {
									set(mleft);
									evalSort();
									if(CHILD(0).isNumber()) {
										nrmul = &CHILD(0).number();
									} else if(CHILD(0).isMultiplication() && CHILD(0).size() > 0 && CHILD(0)[0].isNumber()) {
										nrmul = &CHILD(0)[0].number();
									} else {
										PREPEND(m_one);
										nrmul = &CHILD(0).number();
									}
								} else {
									clear(true);
									m_type = STRUCT_MULTIPLICATION;
									APPEND(m_one);
									APPEND(mleft);
									nrmul = &CHILD(0).number();
								}
								size_t pi = 0;
								for(size_t i = 0; i < zeroes.size(); i++) {
									if(zeroes[i].isInteger()) {
										APPEND(xvar2);
									} else {
										APPEND(m_zero);
									}
									mcur = &CHILD(SIZE - 1);
									if(pi < powers_i.size() && powers_i[pi] == i) {
										mcur->raise(MathStructure(powers[pi], 1L, 0L));
										mcur = &(*mcur)[0];
										if(zeroes[i].isInteger()) {
											mcur->add(zeroes[i]);
										} else {
											Number nr(zeroes[i].denominator());
											mcur->add(zeroes[i].numerator());
											(*mcur)[0] *= xvar2;
											(*mcur)[0][0].number() = nr;
											nr.raise(powers[pi]);
											nrmul->divide(nr);										
										}
										pi++;
									} else {
										if(zeroes[i].isInteger()) {
											mcur->add(zeroes[i]);
										} else {
											nrmul->divide(zeroes[i].denominator());
											mcur->add(zeroes[i].numerator());
											(*mcur)[0] *= xvar2;
											(*mcur)[0][0].number() = zeroes[i].denominator();
										}
									}
								}
								if(CHILD(0).isNumber() && CHILD(0).number().isOne()) {
									ERASE(0);
								} else if(CHILD(0).isMultiplication() && CHILD(0).size() > 0 && CHILD(0)[0].isNumber() && CHILD(0)[0].number().isOne()) {
									if(CHILD(0).size() == 1) {
										ERASE(0);
									} else if(CHILD(0).size() == 2) {
										CHILD(0).setToChild(2, true);
									} else {
										CHILD(0).delChild(1);
									}
								}
								evalSort(true);
								Number dupspow;
								for(size_t i = 0; i < SIZE - 1; i++) {
									mcur = NULL;
									if(CHILD(i).isPower()) {
										if(CHILD(i)[0].isAddition() && CHILD(i)[1].isNumber()) {
											mcur = &CHILD(i)[0];
										}
									} else if(CHILD(i).isAddition()) {
										mcur = &CHILD(i);
									}
									while(mcur && i + 1 < SIZE) {
										if(CHILD(i + 1).isPower()) {
											if(CHILD(i + 1)[0].isAddition() && CHILD(i + 1)[1].isNumber() && mcur->equals(CHILD(i + 1)[0])) {
												dupspow += CHILD(i + 1)[1].number();
											} else {
												mcur = NULL;
											}
										} else if(CHILD(i + 1).isAddition() && mcur->equals(CHILD(i + 1))) {
											dupspow++;
										} else {
											mcur = NULL;
										}
										if(mcur) {
											ERASE(i + 1);
										}
									}
									if(!dupspow.isZero()) {
										if(CHILD(i).isPower()) {
											CHILD(i)[1].number() += dupspow;
										} else {
											dupspow++;
											CHILD(i) ^= dupspow;
										}
										dupspow.clear();
									}
								}
								if(SIZE == 1) {
									setToChild(1, true);
								} else {
									EvaluationOptions eo2 = eo;
									eo2.expand = false;
									calculatesub(eo2, eo2, false);
								}
								evalSort(true);
								return true;
							}
						}
					}
				}

				if(SIZE == 2) {
					Number nr1(1, 1, 0), nr2(1, 1, 0);
					bool b = true;
					bool b1_neg = false, b2_neg = false;
					for(size_t i = 0; i < SIZE && b; i++) {
						b = false;
						if(CHILD(i).isInteger()) {
							b = true;
							if(i == 0) nr1 = CHILD(i).number();
							else nr2 = CHILD(i).number();
						} else if(CHILD(i).isMultiplication() && CHILD(i).size() > 1) {
							b = true;
							size_t i2 = 0;
							if(CHILD(i)[0].isInteger()) {
								if(i == 0) nr1 = CHILD(i)[0].number();
								else nr2 = CHILD(i)[0].number();
								i2++;
							}
							for(; i2 < CHILD(i).size(); i2++) {
								if(!CHILD(i)[i2].isPower() || !CHILD(i)[i2][1].isInteger() || !CHILD(i)[i2][1].number().isPositive() || !CHILD(i)[i2][1].number().isEven() || !CHILD(i)[i2][0].representsNonMatrix()) {
									b = false;
									break;
								}
							}
						} else if(CHILD(i).isPower() && CHILD(i)[1].isNumber() && CHILD(i)[1].number().isInteger() && CHILD(i)[1].number().isPositive() && CHILD(i)[1].number().isEven() && CHILD(i)[0].representsNonMatrix()) {
							b = true;
						}
					}
					if(b) {
						b1_neg = nr1.isNegative();
						b2_neg = nr2.isNegative();
						if(b1_neg == b2_neg) b = false;
					}
					if(b) {
						if(b1_neg) nr1.negate();
						if(!nr1.isOne()) {
							b = nr1.isPerfectSquare();
							if(b) nr1.isqrt();
						}
					}
					if(b) {
						if(b2_neg) nr2.negate();
						if(!nr2.isOne()) {
							b = nr2.isPerfectSquare();
							if(b) nr2.isqrt();
						}
					}
					if(b) {
						MathStructure *mmul = new MathStructure(*this);
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isNumber()) {
								if(i == 0) {
									CHILD(i).number() = nr1;
									if(b1_neg) nr1.negate();
									(*mmul)[i].number() = nr1;
								} else {
									CHILD(i).number() = nr2;
									if(b2_neg) nr2.negate();
									(*mmul)[i].number() = nr2;
								}
							} else if(CHILD(i).isMultiplication() && CHILD(i).size() > 1) {
								b = true;
								size_t i2 = 0;
								if(CHILD(i)[0].isNumber()) {
									if(i == 0) {
										CHILD(i)[0].number() = nr1;
										if(b1_neg) nr1.negate();
										(*mmul)[i][0].number() = nr1;
									} else {
										CHILD(i)[0].number() = nr2;
										if(b2_neg) nr2.negate();
										(*mmul)[i][0].number() = nr2;
									}
									i2++;
								}
								for(; i2 < CHILD(i).size(); i2++) {
									if(CHILD(i)[i2][1].number().isTwo()) {
										CHILD(i)[i2].setToChild(1, true);
										(*mmul)[i][i2].setToChild(1, true);
									} else {
										CHILD(i)[i2][1].number().divide(2);
										(*mmul)[i][i2][1].number().divide(2);
									}
									CHILD(i).childUpdated(i2 + 1);
									(*mmul)[i].childUpdated(i2 + 1);
								}
								if(CHILD(i)[0].isOne()) CHILD(i).delChild(1, true);
								if((*mmul)[i][0].isOne()) (*mmul)[i].delChild(1, true);
							} else if(CHILD(i).isPower()) {
								if(CHILD(i)[1].number().isTwo()) {
									CHILD(i).setToChild(1, true);
									(*mmul)[i].setToChild(1, true);
								} else {
									CHILD(i)[1].number().divide(2);
									(*mmul)[i][1].number().divide(2);
								}
							}
							CHILD_UPDATED(i);
							mmul->childUpdated(i + 1);
						}
						if(recursive) {
							factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
							mmul->factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
						}
						multiply_nocopy(mmul);
						evalSort(true);
						return true;
					}
				}

				//x^3-y^3=(x-y)(x^2+xy+y^2)
				if(SIZE == 2 && CHILD(0).isPower() && CHILD(0)[1].isNumber() && CHILD(0)[1].number() == 3 && CHILD(1).isMultiplication() && CHILD(1).size() == 2 && CHILD(1)[0].isMinusOne() && CHILD(1)[1].isPower() && CHILD(1)[1][1].isNumber() && CHILD(1)[1][1].number() == 3) {
					if(CHILD(0)[0].representsNonMatrix() && CHILD(1)[1][0].representsNonMatrix()) {
						MathStructure *m2 = new MathStructure(*this);
						(*m2)[0].setToChild(1, true);
						(*m2)[1][1].setToChild(1, true);
						EvaluationOptions eo2 = eo;
						eo2.expand = false;
						m2->calculatesub(eo2, eo2, false);					
						CHILD(0)[1].set(2, 1, 0, true);
						CHILD(1).setToChild(2, true);
						CHILD(1)[1].set(2, 1, 0, true);
						MathStructure *m3 = new MathStructure(CHILD(0)[0]);
						m3->calculateMultiply(CHILD(1)[0], eo2);
						add_nocopy(m3, true);
						calculatesub(eo2, eo2, false);
						multiply_nocopy(m2, true);
						evalSort(true);
						return true;
					}
				}
				//-x^3+y^3=(-x+y)(x^2+xy+y^2)
				if(SIZE == 2 && CHILD(1).isPower() && CHILD(1)[1].isNumber() && CHILD(1)[1].number() == 3 && CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isMinusOne() && CHILD(0)[1].isPower() && CHILD(0)[1][1].isNumber() && CHILD(0)[1][1].number() == 3) {
					if(CHILD(1)[0].representsNonMatrix() && CHILD(0)[1][0].representsNonMatrix()) {
						MathStructure *m2 = new MathStructure(*this);
						(*m2)[1].setToChild(1, true);
						(*m2)[0][1].setToChild(1, true);
						EvaluationOptions eo2 = eo;
						eo2.expand = false;
						m2->calculatesub(eo2, eo2, false);
						CHILD(1)[1].set(2, 1, 0, true);
						CHILD(0).setToChild(2, true);
						CHILD(0)[1].set(2, 1, 0, true);
						MathStructure *m3 = new MathStructure(CHILD(0)[0]);
						m3->calculateMultiply(CHILD(1)[0], eo2);
						add_nocopy(m3, true);
						calculatesub(eo2, eo2, false);
						multiply_nocopy(m2, true);
						evalSort(true);
						return true;
					}
				}

				if(!only_integers && !force_factorization.isUndefined()) {
					if(SIZE >= 2) {
						bool b = false;
						MathStructure mdiv(1, 1, 0);
						size_t x_i = 0;
						size_t xp_i = 0;
						for(size_t i2 = 0; i2 < SIZE; i2++) {
							if(CHILD(i2).isPower() && CHILD(i2)[0] == force_factorization && CHILD(i2)[1].isInteger() && CHILD(i2)[1].number().isGreaterThan(nr_two)) {
								b = true;
								xp_i = i2;
								break;
							} else if(CHILD(i2).isMultiplication() && CHILD(i2).size() >= 2) {
								for(size_t i = 0; i < CHILD(i2).size(); i++) {
									if(!b && CHILD(i2)[i].isPower() && CHILD(i2)[i][0] == force_factorization) {
										if(CHILD(i2)[i][1].isInteger() && CHILD(i2)[i][1].number().isGreaterThan(nr_two)) {
											b = true;
											x_i = i;
										} else {
											b = false;
											break;
										}
									} else if(CHILD(i2)[i].contains(force_factorization, true)) {
										b = false;
										break;
									}
								}
								if(b) {
									xp_i = i2;
									mdiv = CHILD(i2);
									mdiv.delChild(x_i + 1, true);
								}
							}
						}
						if(b) {
							for(size_t i = 0; i < SIZE; i++) {
								if(i != xp_i && CHILD(i).contains(force_factorization, true)) {
									b = false;
									break;
								}
							}
						}
						if(b) {
							if(xp_i != 0) SWAP_CHILDREN(xp_i, 0)
							if(!mdiv.isOne()) {
								CHILD(0).setToChild(x_i + 1);
								for(size_t i = 1; i < SIZE; i++) {
									CHILD(i).calculateDivide(mdiv, eo);
								}
								childrenUpdated();
							}
							if(SIZE > 2) {
								MathStructure *m0 = &CHILD(0);
								m0->ref();
								ERASE(0)
								multiply_nocopy(m0);
								SWAP_CHILDREN(0, 1)
							}
							int n = CHILD(0)[1].number().intValue();
							if(n % 4 == 0) {
								int i_u = 1;
								if(n != 4) {
									i_u = n / 4;
								}
								MathStructure m_sqrt2(2, 1, 0);
								m_sqrt2.calculateRaise(nr_half, eo);
								MathStructure m_sqrtb(CHILD(1));
								m_sqrtb.calculateRaise(nr_half, eo);
								MathStructure m_bfourth(CHILD(1));
								m_bfourth.calculateRaise(Number(1, 4), eo);
								m_sqrt2.calculateMultiply(m_bfourth, eo);
								MathStructure m_x(force_factorization);
								if(i_u != 1) m_x ^= i_u;
								m_sqrt2.calculateMultiply(m_x, eo);
								MathStructure *m2 = new MathStructure(force_factorization);
								m2->raise(Number(i_u * 2, 1));
								m2->add(m_sqrtb);
								m2->calculateAdd(m_sqrt2, eo);
								set(force_factorization, true);
								raise(Number(i_u * 2, 1));
								add(m_sqrtb);
								calculateSubtract(m_sqrt2, eo);
								multiply_nocopy(m2);
							} else {
								int i_u = 1;
								if(n % 2 == 0) {
									i_u = 2;
									n /= 2;
								}
								MathStructure *m2 = new MathStructure(CHILD(1));
								m2->calculateRaise(Number(n - 1, n), eo);
								for(int i = 1; i < n - 1; i++) {
									MathStructure *mterm = new MathStructure(CHILD(1));
									mterm->calculateRaise(Number(n - i - 1, n), eo);
									mterm->multiply(force_factorization);
									if(i != 1 || i_u != 1) {
										mterm->last().raise(Number(i * i_u, 1));
										mterm->childUpdated(mterm->size());
									}
									if(i % 2 == 1) mterm->calculateMultiply(m_minus_one, eo);
									m2->add_nocopy(mterm, true);
								}
								MathStructure *mterm = new MathStructure(force_factorization);
								mterm->raise(Number((n - 1) * i_u, 1));
								m2->add_nocopy(mterm, true);
								mterm = new MathStructure(force_factorization);
								if(i_u != 1) mterm->raise(Number(i_u, 1));
								SET_CHILD_MAP(1)
								calculateRaise(Number(1, n), eo);
								add_nocopy(mterm);
								multiply_nocopy(m2);
							}
							if(!mdiv.isOne()) multiply(mdiv, true);
							evalSort(true);
							return true;
						}
					}
				}

				//-x-y = -(x+y)
				bool b = true;
				for(size_t i2 = 0; i2 < SIZE; i2++) {
					if((!CHILD(i2).isNumber() || !CHILD(i2).number().isNegative()) && (!CHILD(i2).isMultiplication() || CHILD(i2).size() < 2 || !CHILD(i2)[0].isNumber() || !CHILD(i2)[0].number().isNegative())) {
						b = false;
						break;
					}
				}
				if(b) {
					for(size_t i2 = 0; i2 < SIZE; i2++) {
						if(CHILD(i2).isNumber()) {
							CHILD(i2).number().negate();
						} else {
							CHILD(i2)[0].number().negate();
							if(CHILD(i2)[0].isOne() && CHILD(i2).size() > 1) {
								CHILD(i2).delChild(1);
								if(CHILD(i2).size() == 1) {
									CHILD(i2).setToChild(1, true);
								}
							}
						}
					}
					multiply(MathStructure(-1, 1, 0));
					CHILD_TO_FRONT(1)
				}
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isMultiplication() && CHILD(i).size() > 1) {
						for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
							if(CHILD(i)[i2].isAddition()) {
								for(size_t i3 = i + 1; i3 < SIZE; i3++) {
									if(CHILD(i3).isMultiplication() && CHILD(i3).size() > 1) {
										for(size_t i4 = 0; i4 < CHILD(i3).size(); i4++) {
											if(CHILD(i3)[i4].isAddition() && CHILD(i3)[i4] == CHILD(i)[i2]) {
												MathStructure *mfac = &CHILD(i)[i2];
												mfac->ref();
												CHILD(i).delChild(i2 + 1, true);
												CHILD(i3).delChild(i4 + 1, true);
												CHILD(i3).ref();
												CHILD(i).add_nocopy(&CHILD(i3));
												CHILD(i).calculateAddLast(eo);
												CHILD(i).multiply_nocopy(mfac);
												CHILD_UPDATED(i)
												delChild(i3, true);
												evalSort(true);
												factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
												return true;
											}
										}
									}
								}
								if(SIZE > 2) {
									MathStructure mtest(*this);
									mtest.delChild(i + 1);
									if(mtest == CHILD(i)[i2]) {
										CHILD(i).delChild(i2 + 1, true);
										SET_CHILD_MAP(i);
										add(m_one, true);
										multiply(mtest);
										evalSort(true);
										factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
										return true;
									}
								}
							}
						}
					}
				}
			}

			//Try factorize combinations of terms
			if(term_combination_levels != 0 && SIZE > 2) {
				int start_index = rand() % SIZE;
				int index = start_index;
				int best_index = -1;
				int run_index = 0;
				int max_run_index = SIZE - 3;
				if(term_combination_levels < -1) {
					run_index = -term_combination_levels - 2;
					max_run_index = run_index;
				} else if(term_combination_levels > 0 && term_combination_levels - 1 < max_run_index) {
					max_run_index = term_combination_levels -1;
				}

				MathStructure mbest;
				do {
					if(CALCULATOR->aborted()) break;
					if(endtime_p && endtime_p->tv_sec > 0) {
#ifndef CLOCK_MONOTONIC
						struct timeval curtime;
						gettimeofday(&curtime, NULL);
						if(curtime.tv_sec > endtime_p->tv_sec || (curtime.tv_sec == endtime_p->tv_sec && curtime.tv_usec > endtime_p->tv_usec)) {
#else
						struct timespec curtime;
						clock_gettime(CLOCK_MONOTONIC, &curtime);
						if(curtime.tv_sec > endtime_p->tv_sec || (curtime.tv_sec == endtime_p->tv_sec && curtime.tv_nsec / 1000 > endtime_p->tv_usec)) {
#endif
							CALCULATOR->error(false, _("Because of time constraints only a limited number of combinations of terms were tried during factorization. Repeat factorization to try other random combinations."), NULL);
							break;
						}
					}

					MathStructure mtest(*this);
					mtest.delChild(index + 1);
					if(mtest.factorize(eo, false, run_index == 0 ? 0 : -1 - run_index, 0, only_integers, false, endtime_p, force_factorization)) {
						bool b = best_index < 0 || (mbest.isAddition() && !mtest.isAddition());
						if(!b && (mtest.isAddition() == mbest.isAddition())) {
							b = mtest.isAddition() && (mtest.size() < mbest.size());
							if(!b && (!mtest.isAddition() || mtest.size() == mbest.size())) {
								size_t c1 = mtest.countTotalChildren() + CHILD(index).countTotalChildren();
								size_t c2 = mbest.countTotalChildren() + CHILD(best_index).countTotalChildren();
								b = (c1 < c2);
								if(c1 == c2) {
									b = (count_powers(mtest) + count_powers(CHILD(index))) < (count_powers(mbest) + count_powers(CHILD(best_index)));
								}
							}
						}
						if(b) {
							mbest = mtest;
							best_index = index;
							if(mbest.isPower()) {
								break;
							}
						}
					}
					index++;
					if(index == (int) SIZE) index = 0;
					if(index == start_index) {
						if(best_index >= 0) {
							break;
						}
						run_index++;
						if(run_index > max_run_index) break;
					}
				} while(true);
				if(best_index >= 0) {
					mbest.add(CHILD(best_index), true);
					set(mbest);
					if(term_combination_levels >= -1 && (run_index > 0 || recursive)) {
						factorize(eo, false, term_combination_levels, 0, only_integers, true, endtime_p, force_factorization);
					}
					return true;
				}
			}
		}
		default: {
			if(term_combination_levels < -1) break;
			bool b = false;

			if(isComparison()) {
				EvaluationOptions eo2 = eo;
				eo2.assume_denominators_nonzero = false;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).factorize(eo2, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization)) {
						CHILD_UPDATED(i);
						b = true;
					}
				}
			} else if(recursive && (recursive > 1 || !isAddition())) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization)) {
						CHILD_UPDATED(i);
						b = true;
					}
				}
			}
			if(b) {
				EvaluationOptions eo2 = eo;
				eo2.expand = false;
				calculatesub(eo2, eo2, false);
				evalSort(true);
				if(isAddition()) {
					for(size_t i = 0; i < SIZE; i++) {
						if(CHILD(i).isMultiplication() && CHILD(i).size() > 1) {
							for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
								if(CHILD(i)[i2].isAddition()) {
									for(size_t i3 = i + 1; i3 < SIZE; i3++) {
										if(CHILD(i3).isMultiplication() && CHILD(i3).size() > 1) {
											for(size_t i4 = 0; i4 < CHILD(i3).size(); i4++) {
												if(CHILD(i3)[i4].isAddition() && CHILD(i3)[i4] == CHILD(i)[i2]) {
													MathStructure *mfac = &CHILD(i)[i2];
													mfac->ref();
													CHILD(i).delChild(i2 + 1, true);
													CHILD(i3).delChild(i4 + 1, true);
													CHILD(i3).ref();
													CHILD(i).add_nocopy(&CHILD(i3));
													CHILD(i).calculateAddLast(eo);
													CHILD(i).multiply_nocopy(mfac);
													CHILD_UPDATED(i)
													delChild(i3 + 1, true);
													evalSort(true);
													factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
													return true;
												}
											}
										}
									}
									if(SIZE > 2) {
										MathStructure mtest(*this);
										mtest.delChild(i + 1);
										if(mtest == CHILD(i)[i2]) {
											CHILD(i).delChild(i2 + 1, true);
											SET_CHILD_MAP(i);
											add(m_one, true);
											multiply(mtest);
											evalSort(true);
											factorize(eo, false, term_combination_levels, 0, only_integers, recursive, endtime_p, force_factorization);
											return true;
										}
									}
								}
							}
						}
					}
				}
				return true;
			}
		}
	}
	return false;
}

void MathStructure::swapChildren(size_t index1, size_t index2) {
	if(index1 > 0 && index2 > 0 && index1 <= SIZE && index2 <= SIZE) {
		SWAP_CHILDREN(index1 - 1, index2 - 1)
	}
}
void MathStructure::childToFront(size_t index) {
	if(index > 0 && index <= SIZE) {
		CHILD_TO_FRONT(index - 1)
	}
}
void MathStructure::addChild(const MathStructure &o) {
	APPEND(o);
}
void MathStructure::addChild_nocopy(MathStructure *o) {
	APPEND_POINTER(o);
}
void MathStructure::delChild(size_t index, bool check_size) {
	if(index > 0 && index <= SIZE) {
		ERASE(index - 1);
		if(check_size) {
			if(SIZE == 1) setToChild(1, true);
			else if(SIZE == 0) clear(true);
		}
	}
}
void MathStructure::insertChild(const MathStructure &o, size_t index) {
	if(index > 0 && index <= v_subs.size()) {
		v_order.insert(v_order.begin() + (index - 1), v_subs.size());
		v_subs.push_back(new MathStructure(o));
		CHILD_UPDATED(index - 1);
	} else {
		addChild(o);
	}
}
void MathStructure::insertChild_nocopy(MathStructure *o, size_t index) {
	if(index > 0 && index <= v_subs.size()) {
		v_order.insert(v_order.begin() + (index - 1), v_subs.size());
		v_subs.push_back(o);
		CHILD_UPDATED(index - 1);
	} else {
		addChild_nocopy(o);
	}
}
void MathStructure::setChild(const MathStructure &o, size_t index, bool merge_precision) {
	if(index > 0 && index <= SIZE) {
		CHILD(index - 1).set(o, merge_precision);
		CHILD_UPDATED(index - 1);
	}
}
void MathStructure::setChild_nocopy(MathStructure *o, size_t index, bool merge_precision) {
	if(index > 0 && index <= SIZE) {
		MathStructure *o_prev = v_subs[v_order[index - 1]];
		if(merge_precision) {
			if(!o->isApproximate() && o_prev->isApproximate()) o->setApproximate(true);
			if(o_prev->precision() >= 0 && (o->precision() < 0 || o_prev->precision() < o->precision())) o->setPrecision(o_prev->precision()); 
		}
		o_prev->unref();
		v_subs[v_order[index - 1]] = o;
		CHILD_UPDATED(index - 1);
	}
}
const MathStructure *MathStructure::getChild(size_t index) const {
	if(index > 0 && index <= v_order.size()) {
		return &CHILD(index - 1);
	}
	return NULL;
}
MathStructure *MathStructure::getChild(size_t index) {
	if(index > 0 && index <= v_order.size()) {
		return &CHILD(index - 1);
	}
	return NULL;
}
size_t MathStructure::countChildren() const {
	return SIZE;
}
size_t MathStructure::size() const {
	return SIZE;
}
const MathStructure *MathStructure::base() const {
	if(m_type == STRUCT_POWER && SIZE >= 1) {
		return &CHILD(0);
	}
	return NULL;
}
const MathStructure *MathStructure::exponent() const {
	if(m_type == STRUCT_POWER && SIZE >= 2) {
		return &CHILD(1);
	}
	return NULL;
}
MathStructure *MathStructure::base() {
	if(m_type == STRUCT_POWER && SIZE >= 1) {
		return &CHILD(0);
	}
	return NULL;
}
MathStructure *MathStructure::exponent() {
	if(m_type == STRUCT_POWER && SIZE >= 2) {
		return &CHILD(1);
	}
	return NULL;
}

StructureType MathStructure::type() const {
	return m_type;
}
void MathStructure::unformat(const EvaluationOptions &eo) {
	for(size_t i = 0; i < SIZE; i++) {
		CHILD(i).unformat(eo);
	}
	switch(m_type) {
		case STRUCT_INVERSE: {
			APPEND(m_minus_one);
			m_type = STRUCT_POWER;	
		}
		case STRUCT_NEGATE: {
			PREPEND(m_minus_one);
			m_type = STRUCT_MULTIPLICATION;
		}
		case STRUCT_DIVISION: {
			CHILD(1).raise(m_minus_one);
			m_type = STRUCT_MULTIPLICATION;
		}
		case STRUCT_UNIT: {
			if(o_prefix && !eo.keep_prefixes) {
				if(o_prefix == CALCULATOR->decimal_null_prefix || o_prefix == CALCULATOR->binary_null_prefix) {
					o_prefix = NULL;
				} else {
					Unit *u = o_unit;
					Prefix *p = o_prefix;
					set(p->value());
					multiply(u);
				}
				unformat(eo);
				break;
			} else if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
				set(((CompositeUnit*) o_unit)->generateMathStructure(false, eo.keep_prefixes));
				unformat(eo);
				break;
			}
			b_plural = false;
		}
		default: {}
	}
}

void idm1(const MathStructure &mnum, bool &bfrac, bool &bint) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			if((!bfrac || bint) && mnum.number().isRational()) {
				if(!mnum.number().isInteger()) {
					bint = false;
					bfrac = true;
				}
			} else {
				bint = false;
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if((!bfrac || bint) && mnum.size() > 0 && mnum[0].isNumber() && mnum[0].number().isRational()) {
				if(!mnum[0].number().isInteger()) {
					bint = false;
					bfrac = true;
				}
				
			} else {
				bint = false;
			}
			break;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < mnum.size() && (!bfrac || bint); i++) {
				idm1(mnum[i], bfrac, bint);
			}
			break;
		}
		default: {
			bint = false;
		}
	}
}
void idm2(const MathStructure &mnum, bool &bfrac, bool &bint, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			if(mnum.number().isRational()) {
				if(mnum.number().isInteger()) {
					if(bint) {
						if(mnum.number().isOne()) {
							bint = false;
						} else if(nr.isOne()) {
							nr = mnum.number();
						} else if(nr != mnum.number()) {
							nr.gcd(mnum.number());
							if(nr.isOne()) bint = false;
						}
					}
				} else {
					if(nr.isOne()) {
						nr = mnum.number().denominator();
					} else {
						Number nden(mnum.number().denominator());
						if(nr != nden) {
							Number ngcd(nden);
							ngcd.gcd(nr);
							nden /= ngcd;
							nr *= nden;
						}
					}
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(mnum.size() > 0 && mnum[0].isNumber() && mnum[0].number().isRational()) {
				if(mnum[0].number().isInteger()) {
					if(bint) {
						if(mnum[0].number().isOne()) {
							bint = false;
						} else if(nr.isOne()) {
							nr = mnum[0].number();
						} else if(nr != mnum[0].number()) {
							nr.gcd(mnum[0].number());
							if(nr.isOne()) bint = false;
						}
					}
				} else {
					if(nr.isOne()) {
						nr = mnum[0].number().denominator();
					} else {
						Number nden(mnum[0].number().denominator());
						if(nr != nden) {
							Number ngcd(nden);
							ngcd.gcd(nr);
							nden /= ngcd;
							nr *= nden;
						}
					}
				}
			}
			break;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < mnum.size() && (bfrac || bint); i++) {
				idm2(mnum[i], bfrac, bint, nr);
			}
			break;
		}
		default: {}
	}
}
void idm3(MathStructure &mnum, Number &nr, bool expand) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			mnum.number() *= nr;
			mnum.numberUpdated();
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(mnum.size() > 0 && mnum[0].isNumber()) {
				mnum[0].number() *= nr;
				if(mnum[0].number().isOne() && mnum.size() != 1) {
					mnum.delChild(1);
					if(mnum.size() == 1) mnum.setToChild(1, true);
				}
				return;
			} else if(expand) {
				for(size_t i = 0; i < mnum.size(); i++) {
					if(mnum[i].isAddition()) {
						idm3(mnum[i], nr, true);
						return;
					}
				}
			}
			mnum.insertChild(nr, 1);
			break;
		}
		case STRUCT_ADDITION: {
			if(expand) {
				for(size_t i = 0; i < mnum.size(); i++) {
					idm3(mnum[i], nr, true);
				}
				break;
			}
		}
		default: {
			mnum.transform(STRUCT_MULTIPLICATION);
			mnum.insertChild(nr, 1);
		}
	}
}

bool is_unit_multiexp(const MathStructure &mstruct) {
	if(mstruct.isUnit_exp()) return true;
	if(mstruct.isMultiplication()) {
		for(size_t i3 = 0; i3 < mstruct.size(); i3++) {
			if(!mstruct[i3].isUnit_exp()) {
				return false;
				break;
			}
		}
		return true;
	}
	if(mstruct.isPower() && mstruct[0].isMultiplication()) {
		for(size_t i3 = 0; i3 < mstruct[0].size(); i3++) {
			if(!mstruct[0][i3].isUnit_exp()) {
				return false;
				break;
			}
		}
		return true;
	}
	return false;
}
bool MathStructure::improve_division_multipliers(const PrintOptions &po) {
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {
			size_t inum = 0, iden = 0;
			bool bfrac = false, bint = true, bdiv = false, bnonunitdiv = false;
			size_t index1 = 0, index2 = 0;
			bool dofrac = !po.negative_exponents;
			for(size_t i2 = 0; i2 < SIZE; i2++) {
				if(CHILD(i2).isPower() && CHILD(i2)[1].isMinusOne()) {
					if(!po.place_units_separately || !is_unit_multiexp(CHILD(i2)[0])) {
						if(iden == 0) index1 = i2;
						iden++;
						bdiv = true;
						if(!CHILD(i2)[0].isUnit()) bnonunitdiv = true;
						if(CHILD(i2)[0].containsType(STRUCT_ADDITION)) {
							dofrac = true;
						}
					}
				} else if(!bdiv && !po.negative_exponents && CHILD(i2).isPower() && CHILD(i2)[1].hasNegativeSign()) {
					if(!po.place_units_separately || !is_unit_multiexp(CHILD(i2)[0])) {
						if(!bdiv) index1 = i2;
						bdiv = true;
						if(!CHILD(i2)[0].isUnit()) bnonunitdiv = true;
					}
				} else {
					if(!po.place_units_separately || !is_unit_multiexp(CHILD(i2))) {
						if(inum == 0) index2 = i2;
						inum++;
					}
				}
			}
			if(!bdiv || !bnonunitdiv) break;
			if(iden > 1 && !po.negative_exponents) {
				size_t i2 = index1 + 1;
				while(i2 < SIZE) {
					if(CHILD(i2).isPower() && CHILD(i2)[1].isMinusOne()) {
						CHILD(index1)[0].multiply(CHILD(i2)[0], true);
						ERASE(i2);
					} else {
						i2++;
					}
				}
				iden = 1;
			}
			if(bint) bint = inum > 0 && iden == 1;
			if(inum > 0) idm1(CHILD(index2), bfrac, bint);
			if(iden > 0) idm1(CHILD(index1)[0], bfrac, bint);
			bool b = false;
			if(!dofrac) bfrac = false;
			if(bint || bfrac) {
				Number nr(1, 1);
				if(inum > 0) idm2(CHILD(index2), bfrac, bint, nr);
				if(iden > 0) idm2(CHILD(index1)[0], bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					if(bint) {
						nr.recip();
					}
					if(inum == 0) {
						PREPEND(MathStructure(nr));
						index1 += 1;
					} else if(inum > 1 && !CHILD(index2).isNumber()) {
						idm3(*this, nr, !po.allow_factorization);
					} else {
						idm3(CHILD(index2), nr, !po.allow_factorization);
					}
					if(iden > 0) {
						idm3(CHILD(index1)[0], nr, !po.allow_factorization);
					} else {
						MathStructure mstruct(nr);
						mstruct.raise(m_minus_one);
						insertChild(mstruct, index1);
					}
					b = true;
				}
			}
			/*if(!po.negative_exponents && SIZE == 2 && CHILD(1).isAddition()) {
				MathStructure factor_mstruct(1, 1);
				if(factorize_find_multiplier(CHILD(1), CHILD(1), factor_mstruct)) {
					transform(STRUCT_MULTIPLICATION);
					PREPEND(factor_mstruct);
				}
			}*/
			return b;
		}
		case STRUCT_DIVISION: {
			bool bint = true, bfrac = false;
			idm1(CHILD(0), bfrac, bint);
			idm1(CHILD(1), bfrac, bint);
			if(bint || bfrac) {
				Number nr(1, 1);
				idm2(CHILD(0), bfrac, bint, nr);
				idm2(CHILD(1), bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					if(bint) {
						nr.recip();
					}
					idm3(CHILD(0), nr, !po.allow_factorization);
					idm3(CHILD(1), nr, !po.allow_factorization);
					return true;
				}
			}
			break;
		}
		case STRUCT_INVERSE: {
			bool bint = false, bfrac = false;
			idm1(CHILD(0), bfrac, bint);
			if(bint || bfrac) {
				Number nr(1, 1);
				idm2(CHILD(0), bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					setToChild(1, true);
					idm3(*this, nr, !po.allow_factorization);
					transform_nocopy(STRUCT_DIVISION, new MathStructure(nr));
					SWAP_CHILDREN(0, 1);
					return true;
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(1).isMinusOne()) {
				bool bint = false, bfrac = false;
				idm1(CHILD(0), bfrac, bint);
				if(bint || bfrac) {
					Number nr(1, 1);
					idm2(CHILD(0), bfrac, bint, nr);
					if((bint || bfrac) && !nr.isOne()) {
						idm3(CHILD(0), nr, !po.allow_factorization);
						transform(STRUCT_MULTIPLICATION);
						PREPEND(MathStructure(nr));
						return true;
					}
				}
				break;
			}
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).improve_division_multipliers()) b = true;
			}
			return b;
		}
	}
	return false;
}

bool use_prefix_with_unit(Unit *u, const PrintOptions &po) {
	if(po.prefix) return true;
	if(u->isCurrency()) return po.use_prefixes_for_currencies;
	if(po.use_prefixes_for_all_units) return true;
	return u->useWithPrefixesByDefault();
}
bool use_prefix_with_unit(const MathStructure &mstruct, const PrintOptions &po) {	
	if(mstruct.isUnit()) return use_prefix_with_unit(mstruct.unit(), po);
	if(mstruct.isUnit_exp()) return use_prefix_with_unit(mstruct[0].unit(), po);
	return false;
}

bool has_prefix(const MathStructure &mstruct) {
	if(mstruct.isUnit()) return mstruct.prefix() != NULL;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(has_prefix(mstruct[i])) return true;
	}
	return false;
}

void MathStructure::setPrefixes(const PrintOptions &po, MathStructure *parent, size_t pindex) {
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			size_t i = SIZE, im = 0;
			bool b_im = false;
			for(size_t i2 = 0; i2 < SIZE; i2++) {
				if(CHILD(i2).isUnit_exp()) {
					if(CHILD(i2).unit_exp_prefix()) {
						b = false;
						return;
					}
					if(!b) {
						if(use_prefix_with_unit(CHILD(i2), po)) {
							b = true;
							if(i > i2) {i = i2; b_im = false;}
						} else if(i < i2) {
							i = i2;
							b_im = false;
						}
					}
				} else if(CHILD(i2).isPower() && CHILD(i2)[0].isMultiplication()) {
					for(size_t i3 = 0; i3 < CHILD(i2)[0].size(); i3++) {
						if(CHILD(i2)[0][i3].isUnit_exp()) {
							if(CHILD(i2)[0][i3].unit_exp_prefix()) {
								b = false;
								return;
							}
							if(!b) {
								if(use_prefix_with_unit(CHILD(i2)[0][i3], po)) {
									b = true;
									if(i > i2) {
										i = i2;
										im = i3;
										b_im = true;
									}
									break;
								} else if(i < i2 || (i == i2 && im < i3)) {
									i = i2;
									im = i3;
									b_im = true;
								}
							}
						}
					}
				}
			}
			if(b) {
				Number exp(1, 1);
				Number exp2(1, 1);
				bool b2 = false;
				MathStructure *munit = NULL, *munit2 = NULL;
				if(b_im) munit = &CHILD(i)[0][im];
				else munit = &CHILD(i);
				if(CHILD(i).isPower()) {
					if(CHILD(i)[1].isNumber() && CHILD(i)[1].number().isInteger() && !CHILD(i)[1].number().isZero()) {
						if(b_im && munit->isPower()) {
							if((*munit)[1].isNumber() && (*munit)[1].number().isInteger() && !(*munit)[1].number().isZero()) {
								exp = CHILD(i)[1].number();
								exp *= (*munit)[1].number();
							} else {
								b = false;
							}
						} else {
							exp = CHILD(i)[1].number();
						}
					} else {
						b = false;
					}
				}
				if(po.use_denominator_prefix && !exp.isNegative()) {
					for(size_t i2 = i + 1; i2 < SIZE; i2++) {
						if(CALCULATOR->aborted()) break;
						if(CHILD(i2).isPower() && CHILD(i2)[1].isNumber() && CHILD(i2)[1].number().isNegative()) {
							if(CHILD(i2)[0].isUnit() && use_prefix_with_unit(CHILD(i2)[0], po)) {
								munit2 = &CHILD(i2)[0];
								if(munit2->prefix() || !CHILD(i2)[1].number().isInteger()) {
									break;
								}
								if(!b) {
									b = true;
									exp = CHILD(i2)[1].number();
									munit = munit2;
								} else {
									b2 = true;
									exp2 = CHILD(i2)[1].number();
								}
								break;
							} else if(CHILD(i2)[0].isMultiplication()) {
								bool b_break = false;
								for(size_t im2 = 0; im2 < CHILD(i2)[0].size(); im2++) {
									if(CHILD(i2)[0][im2].isUnit_exp() && use_prefix_with_unit(CHILD(i2)[0][im2], po) && (CHILD(i2)[0][im2].isUnit() || (CHILD(i2)[0][im2][1].isNumber() && (CHILD(i2)[0][im2][1].number().isPositive() || (!b && CHILD(i2)[0][im2][1].number().isNegative())) && CHILD(i2)[0][im2][1].number().isInteger()))) {
										Number exp_multi(1);
										if(CHILD(i2)[0][im2].isUnit()) {
											munit2 = &CHILD(i2)[0][im2];
										} else {
											munit2 = &CHILD(i2)[0][im2][0];
											exp_multi = CHILD(i2)[0][im2][1].number();
										}
										b_break = true;
										if(munit2->prefix() || !CHILD(i2)[1].number().isInteger()) {
											break;
										}
										if(!b) {
											b = true;
											exp = CHILD(i2)[1].number();
											exp *= exp_multi;
											i = i2;
										} else {
											b2 = true;
											exp2 = CHILD(i2)[1].number();
											exp2 *= exp_multi;
										}
										break;
									}
								}
								if(b_break) break;
							}
						}
					}
				} else if(exp.isNegative() && b) {
					bool had_unit = false;
					for(size_t i2 = i + 1; i2 < SIZE; i2++) {
						if(CALCULATOR->aborted()) break;
						bool b3 = false;
						if(CHILD(i2).isPower() && CHILD(i2)[1].isNumber() && CHILD(i2)[1].number().isPositive()) {
							if(CHILD(i2)[0].isUnit()) {
								if(!use_prefix_with_unit(CHILD(i2)[0], po)) {
									had_unit = true;
								} else {
									munit2 = &CHILD(i2);
									if(munit2->prefix() || !CHILD(i2)[1].number().isInteger()) {
										break;
									}
									b3 = true;
									exp2 = exp;
									exp = CHILD(i2)[1].number();
								}
							} else if(CHILD(i2)[0].isMultiplication()) {
								bool b_break = false;
								for(size_t im2 = 0; im2 < CHILD(i2)[0].size(); im2++) {
									if(CHILD(i2)[0][im2].isUnit_exp() && (CHILD(i2)[0][im2].isUnit() || (CHILD(i2)[0][im2][1].isNumber() && CHILD(i2)[0][im2][1].number().isPositive() && CHILD(i2)[0][im2][1].number().isInteger()))) {
										if(!use_prefix_with_unit(CHILD(i2)[0][im2], po)) {
											had_unit = true;
										} else {
											Number exp_multi(1);
											if(CHILD(i2)[0][im2].isUnit()) {
												munit2 = &CHILD(i2)[0][im2];
											} else {
												munit2 = &CHILD(i2)[0][im2][0];
												exp_multi = CHILD(i2)[0][im2][1].number();
											}
											b_break = true;
											if(munit2->prefix() || !CHILD(i2)[1].number().isInteger()) {
												break;
											}
											exp2 = exp;
											exp = CHILD(i2)[1].number();
											exp *= exp_multi;
											b3 = true;
											break;
										}
									}
								}
								if(b_break) break;
							}
						} else if(CHILD(i2).isUnit()) {
							if(!use_prefix_with_unit(CHILD(i2), po)) {
								had_unit = true;
							} else {
								if(CHILD(i2).prefix()) break;
								exp2 = exp;
								exp.set(1, 1, 0);
								b3 = true;
								munit2 = &CHILD(i2);
							}
						}
						if(b3) {
							if(po.use_denominator_prefix) {
								b2 = true;
								MathStructure *munit3 = munit;
								munit = munit2;
								munit2 = munit3;
							} else {
								munit = munit2;
							}
							had_unit = false;
							break;
						}
					}
					if(had_unit && !po.use_denominator_prefix) b = false;
				}
				Number exp10;
				if(b) {
					if(po.prefix) {
						if(po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix) {
							if(munit->isUnit()) munit->setPrefix(po.prefix);
							else (*munit)[0].setPrefix(po.prefix);
							if(CHILD(0).isNumber()) {
								CHILD(0).number() /= po.prefix->value(exp);
							} else {
								PREPEND(po.prefix->value(exp));
								CHILD(0).number().recip();
							}
						}
					} else if(po.use_unit_prefixes && CHILD(0).isNumber() && exp.isInteger()) {
						exp10 = CHILD(0).number();
						exp10.abs();
						exp10.intervalToMidValue();
						if(exp10.isLessThanOrEqualTo(Number(1, 1, 1000)) && exp10.isGreaterThanOrEqualTo(Number(1, 1, -1000))) {
							exp10.log(10);
							exp10.intervalToMidValue();
							exp10.floor();
							if(b2) {
								Number tmp_exp(exp10);
								tmp_exp.setNegative(false);
								Number e1(3, 1, 0);
								e1 *= exp;
								Number e2(3, 1, 0);
								e2 *= exp2;
								e2.setNegative(false);
								int i4 = 0;
								while(true) {
									tmp_exp -= e1;
									if(!tmp_exp.isPositive()) {
										break;
									}
									if(exp10.isNegative()) i4++;
									tmp_exp -= e2;
									if(tmp_exp.isNegative()) {
										break;
									}
									if(!exp10.isNegative())	i4++;
								}
								e2.setNegative(exp10.isNegative());
								e2 *= i4;
								exp10 -= e2;
							}
							DecimalPrefix *p = CALCULATOR->getBestDecimalPrefix(exp10, exp, po.use_all_prefixes);
							if(p) {
								Number test_exp(exp10);
								test_exp -= p->exponent(exp);
								if(test_exp.isInteger()) {
									if((exp10.isPositive() && exp10.compare(test_exp) == COMPARISON_RESULT_LESS) || (exp10.isNegative() && exp10.compare(test_exp) == COMPARISON_RESULT_GREATER)) {
										CHILD(0).number() /= p->value(exp);
										if(munit->isUnit()) munit->setPrefix(p);
										else (*munit)[0].setPrefix(p);
									}
								}
							}
						}
					}
					if(b2 && CHILD(0).isNumber() && (po.prefix || po.use_unit_prefixes) && (po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix)) {
						exp10 = CHILD(0).number();
						exp10.abs();
						exp10.intervalToMidValue();
						if(exp10.isLessThanOrEqualTo(Number(1, 1, 1000)) && exp10.isGreaterThanOrEqualTo(Number(1, 1, -1000))) {
							exp10.log(10);
							exp10.intervalToMidValue();
							exp10.floor();
							DecimalPrefix *p = CALCULATOR->getBestDecimalPrefix(exp10, exp2, po.use_all_prefixes);
							if(p) {
								Number test_exp(exp10);
								test_exp -= p->exponent(exp2);
								if(test_exp.isInteger()) {
									if((exp10.isPositive() && exp10.compare(test_exp) == COMPARISON_RESULT_LESS) || (exp10.isNegative() && exp10.compare(test_exp) == COMPARISON_RESULT_GREATER)) {
										CHILD(0).number() /= p->value(exp2);
										if(munit2->isUnit()) munit2->setPrefix(p);
										else (*munit2)[0].setPrefix(p);
									}
								}
							}
						}
					}	
				}
				break;
			}
		}
		case STRUCT_UNIT: {
			if(!o_prefix && (po.prefix && po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix)) {
				transform(STRUCT_MULTIPLICATION, m_one);
				SWAP_CHILDREN(0, 1);
				setPrefixes(po, parent, pindex);
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(0).isUnit()) {
				if(CHILD(1).isNumber() && CHILD(1).number().isReal() && !CHILD(0).prefix() && !o_prefix && (po.prefix && po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix)) {
					transform(STRUCT_MULTIPLICATION, m_one);
					SWAP_CHILDREN(0, 1);
					setPrefixes(po, parent, pindex);
				}
				break;
			}
		}
		default: {
			if(po.prefix || !has_prefix(*this)) {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).setPrefixes(po, this, i + 1);
				}
			}
		}
	}
}
bool split_unit_powers(MathStructure &mstruct);
bool split_unit_powers(MathStructure &mstruct) {
	bool b = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(CALCULATOR->aborted()) break;
		if(split_unit_powers(mstruct[i])) {
			mstruct.childUpdated(i + 1);
			b = true;
		}
	}
	if(mstruct.isPower() && mstruct[0].isMultiplication()) {
		bool b2 = mstruct[1].isNumber();
		for(size_t i = 0; i < mstruct[0].size(); i++) {
			if(mstruct[0][i].isPower() && (!b2 || !mstruct[0][i][1].isNumber())) return b;
		}
		MathStructure mpower(mstruct[1]);
		mstruct.setToChild(1);
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isPower()) mstruct[i][1].number() *= mpower.number();
			else mstruct[i].raise(mpower);
		}
		mstruct.childrenUpdated();
		return true;
	}
	return b;
}
void MathStructure::postFormatUnits(const PrintOptions &po, MathStructure*, size_t) {
	switch(m_type) {
		case STRUCT_DIVISION: {
			if(po.place_units_separately) {
				vector<size_t> nums;
				bool b1 = false, b2 = false;
				if(CHILD(0).isMultiplication()) {
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CHILD(0)[i].isUnit_exp()) {
							nums.push_back(i);
						} else {
							b1 = true;
						}
					}
					b1 = b1 && !nums.empty();
				} else if(CHILD(0).isUnit_exp()) {
					b1 = true;
				}
				vector<size_t> dens;
				if(CHILD(1).isMultiplication()) {
					for(size_t i = 0; i < CHILD(1).size(); i++) {
						if(CHILD(1)[i].isUnit_exp()) {
							dens.push_back(i);
						} else {
							b2 = true;
						}
					}
					b2 = b2 && !dens.empty();
				} else if(CHILD(1).isUnit_exp()) {
					if(CHILD(0).isUnit_exp()) {
						b1 = false;
					} else {
						b2 = true;
					}
				}
				if(b2 && !b1) b1 = true;
				if(b1) {
					MathStructure num = m_undefined;
					if(CHILD(0).isUnit_exp()) {
						num = CHILD(0);
						CHILD(0).set(m_one);
					} else if(nums.size() > 0) {
						num = CHILD(0)[nums[0]];
						for(size_t i = 1; i < nums.size(); i++) {
							num.multiply(CHILD(0)[nums[i]], i > 1);
						}
						for(size_t i = 0; i < nums.size(); i++) {
							CHILD(0).delChild(nums[i] + 1 - i);
						}
						if(CHILD(0).size() == 1) {
							CHILD(0).setToChild(1, true);
						}
					}
					MathStructure den = m_undefined;
					if(CHILD(1).isUnit_exp()) {
						den = CHILD(1);
						setToChild(1, true);
					} else if(dens.size() > 0) {
						den = CHILD(1)[dens[0]];
						for(size_t i = 1; i < dens.size(); i++) {
							den.multiply(CHILD(1)[dens[i]], i > 1);
						}
						for(size_t i = 0; i < dens.size(); i++) {
							CHILD(1).delChild(dens[i] + 1 - i);
						}
						if(CHILD(1).size() == 1) {
							CHILD(1).setToChild(1, true);
						}
					}
					if(num.isUndefined()) {
						transform(STRUCT_DIVISION, den);
					} else {
						if(!den.isUndefined()) {
							num.transform(STRUCT_DIVISION, den);
						}
						multiply(num, false);
					}
					if(CHILD(0).isDivision()) {
						if(CHILD(0)[0].isMultiplication()) {
							if(CHILD(0)[0].size() == 1) {
								CHILD(0)[0].setToChild(1, true);
							} else if(CHILD(0)[0].size() == 0) {
								CHILD(0)[0] = 1;
							}
						}
						if(CHILD(0)[1].isMultiplication()) {
							if(CHILD(0)[1].size() == 1) {
								CHILD(0)[1].setToChild(1, true);
							} else if(CHILD(0)[1].size() == 0) {
								CHILD(0).setToChild(1, true);
							}
						} else if(CHILD(0)[1].isOne()) {
							CHILD(0).setToChild(1, true);
						}
						if(CHILD(0).isDivision() && CHILD(0)[1].isNumber() && CHILD(0)[0].isMultiplication() && CHILD(0)[0].size() > 1 && CHILD(0)[0][0].isNumber()) {
							MathStructure *msave = new MathStructure;
							if(CHILD(0)[0].size() == 2) {
								msave->set(CHILD(0)[0][1]);
								CHILD(0)[0].setToChild(1, true);
							} else {
								msave->set(CHILD(0)[0]);
								CHILD(0)[0].setToChild(1, true);
								msave->delChild(1);
							}
							if(isMultiplication()) {
								insertChild_nocopy(msave, 2);
							} else {
								CHILD(0).multiply_nocopy(msave);
							}
						}
					}
					bool do_plural = po.short_multiplication;
					CHILD(0).postFormatUnits(po, this, 1);
					CHILD_UPDATED(0);
					switch(CHILD(0).type()) {
						case STRUCT_NUMBER: {
							if(CHILD(0).isZero() || CHILD(0).number().isOne() || CHILD(0).number().isMinusOne() || CHILD(0).number().isFraction()) {
								do_plural = false;
							}
							break;
						}
						case STRUCT_DIVISION: {
							if(CHILD(0)[0].isNumber() && CHILD(0)[1].isNumber()) {
								if(CHILD(0)[0].number().isLessThanOrEqualTo(CHILD(0)[1].number())) {
									do_plural = false;
								}
							}
							break;
						}
						case STRUCT_INVERSE: {
							if(CHILD(0)[0].isNumber() && CHILD(0)[0].number().isGreaterThanOrEqualTo(1)) {
								do_plural = false;
							}
							break;
						}
						default: {}
					}
					split_unit_powers(CHILD(1));
					switch(CHILD(1).type()) {
						case STRUCT_UNIT: {
							CHILD(1).setPlural(do_plural);
							break;
						}
						case STRUCT_POWER: {
							CHILD(1)[0].setPlural(do_plural);
							break;
						}
						case STRUCT_MULTIPLICATION: {
							if(po.limit_implicit_multiplication) CHILD(1)[0].setPlural(do_plural);
							else CHILD(1)[CHILD(1).size() - 1].setPlural(do_plural);
							break;
						}
						case STRUCT_DIVISION: {
							switch(CHILD(1)[0].type()) {
								case STRUCT_UNIT: {
									CHILD(1)[0].setPlural(do_plural);
									break;
								}
								case STRUCT_POWER: {
									CHILD(1)[0][0].setPlural(do_plural);
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(po.limit_implicit_multiplication) CHILD(1)[0][0].setPlural(do_plural);
									else CHILD(1)[0][CHILD(1)[0].size() - 1].setPlural(do_plural);
									break;
								}
								default: {}
							}
							break;
						}
						default: {}
					}
				}
			} else {
				for(size_t i = 0; i < SIZE; i++) {
					if(CALCULATOR->aborted()) break;
					CHILD(i).postFormatUnits(po, this, i + 1);
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_UNIT: {
			b_plural = false;
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(SIZE > 1 && CHILD(1).isUnit_exp() && CHILD(0).isNumber()) {
				bool do_plural = po.short_multiplication && !(CHILD(0).isZero() || CHILD(0).number().isOne() || CHILD(0).number().isMinusOne() || CHILD(0).number().isFraction());
				size_t i = 2;
				for(; i < SIZE; i++) {
					if(CALCULATOR->aborted()) break;
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(false);
					} else if(CHILD(i).isPower() && CHILD(i)[0].isUnit()) {
						CHILD(i)[0].setPlural(false);
					} else {
						break;
					}
				}
				if(do_plural) {
					if(po.limit_implicit_multiplication) i = 1;
					else i--;
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(true);
					} else {
						CHILD(i)[0].setPlural(true);
					}
				}
			} else if(SIZE > 0) {
				int last_unit = -1;
				for(size_t i = 0; i < SIZE; i++) {
					if(CALCULATOR->aborted()) break;
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(false);
						if(!po.limit_implicit_multiplication || last_unit < 0) {
							last_unit = i;
						}
					} else if(CHILD(i).isPower() && CHILD(i)[0].isUnit()) {
						CHILD(i)[0].setPlural(false);
						if(!po.limit_implicit_multiplication || last_unit < 0) {
							last_unit = i;
						}
					} else if(last_unit >= 0) {
						break;
					}
				}
				if(po.short_multiplication && last_unit > 0) {
					if(CHILD(last_unit).isUnit()) {
						CHILD(last_unit).setPlural(true);
					} else {
						CHILD(last_unit)[0].setPlural(true);
					}
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(0).isUnit()) {
				CHILD(0).setPlural(false);
				break;
			}
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).postFormatUnits(po, this, i + 1);
				CHILD_UPDATED(i);
			}
		}
	}
}
bool MathStructure::factorizeUnits() {
	switch(m_type) {
		case STRUCT_ADDITION: {
			bool b = false;
			MathStructure mstruct_units(*this);
			MathStructure mstruct_new(*this);
			for(size_t i = 0; i < mstruct_units.size(); i++) {
				if(CALCULATOR->aborted()) break;
				if(mstruct_units[i].isMultiplication()) {
					for(size_t i2 = 0; i2 < mstruct_units[i].size();) {
						if(CALCULATOR->aborted()) break;
						if(!mstruct_units[i][i2].isUnit_exp()) {
							mstruct_units[i].delChild(i2 + 1);
						} else {
							i2++;
						}
					}
					if(mstruct_units[i].size() == 0) mstruct_units[i].setUndefined();
					else if(mstruct_units[i].size() == 1) mstruct_units[i].setToChild(1);
					for(size_t i2 = 0; i2 < mstruct_new[i].size();) {
						if(CALCULATOR->aborted()) break;
						if(mstruct_new[i][i2].isUnit_exp()) {
							mstruct_new[i].delChild(i2 + 1);
						} else {
							i2++;
						}
					}
					if(mstruct_new[i].size() == 0) mstruct_new[i].set(1, 1, 0);
					else if(mstruct_new[i].size() == 1) mstruct_new[i].setToChild(1);
				} else if(mstruct_units[i].isUnit_exp()) {
					mstruct_new[i].set(1, 1, 0);
				} else {
					mstruct_units[i].setUndefined();
				}
			}
			for(size_t i = 0; i < mstruct_units.size(); i++) {
				if(CALCULATOR->aborted()) break;
				if(!mstruct_units[i].isUndefined()) {
					for(size_t i2 = i + 1; i2 < mstruct_units.size();) {
						if(mstruct_units[i2] == mstruct_units[i]) {
							mstruct_new[i].add(mstruct_new[i2], true);
							mstruct_new.delChild(i2 + 1);
							mstruct_units.delChild(i2 + 1);
							b = true;
						} else {
							i2++;
						}
					}
					if(mstruct_new[i].isOne()) mstruct_new[i].set(mstruct_units[i]);
					else mstruct_new[i].multiply(mstruct_units[i], true);
				}
			}
			if(b) {
				if(mstruct_new.size() == 1) set(mstruct_new[0], true);
				else set(mstruct_new, true);
				return true;
			}
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).factorizeUnits()) {
					CHILD_UPDATED(i);
					b = true;
				}
			}
			return b;
		}
	}
}
void MathStructure::prefixCurrencies() {
	if(isMultiplication() && (!hasNegativeSign() || CALCULATOR->place_currency_code_before_negative)) {
		int index = -1;
		for(size_t i = 0; i < SIZE; i++) {
			if(CALCULATOR->aborted()) break;
			if(CHILD(i).isUnit_exp()) {
				if(CHILD(i).isUnit() && CHILD(i).unit()->isCurrency()) {
					if(index >= 0) {
						index = -1;
						break;
					}
					index = i;
				} else {
					index = -1;
					break;
				}
			}
		}
		if(index >= 0) {
			v_order.insert(v_order.begin(), v_order[index]);
			v_order.erase(v_order.begin() + (index + 1));
		}
	} else {
		for(size_t i = 0; i < SIZE; i++) {
			if(CALCULATOR->aborted()) break;
			CHILD(i).prefixCurrencies();
		}
	}
}
void remove_multi_one(MathStructure &mstruct) {
	if(mstruct.isMultiplication() && mstruct.size() > 1) {
		if(mstruct[0].isOne() && !mstruct[1].isUnit_exp()) {
			if(mstruct.size() == 2) mstruct.setToChild(2, true);
			else mstruct.delChild(1);
		}
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		remove_multi_one(mstruct[i]);
	}
}
bool unnegate_multiplier(MathStructure &mstruct, const PrintOptions &po) {
	if(mstruct.isMultiplication() && mstruct.size() >= 2 && mstruct[0].isNumber() && mstruct[0].number().isNegative()) {
		for(size_t i = 1; i < mstruct.size(); i++) {
			if(mstruct[i].isAddition() || (mstruct[i].isPower() && mstruct[i][0].isAddition() && mstruct[i][1].isMinusOne())) {
				MathStructure *mden;
				if(mstruct[i].isAddition()) mden = &mstruct[i];
				else mden = &mstruct[i][0];
				bool b_pos = false, b_neg = false;
				for(size_t i2 = 0; i2 < mden->size(); i2++) {
					if((*mden)[i2].hasNegativeSign()) {
						b_neg = true;
					} else {
						b_pos = true;
					}
					if(b_neg && b_pos) break;
				}
				if(b_neg && b_pos) {
					for(size_t i2 = 0; i2 < mden->size(); i2++) {
						if((*mden)[i2].isNumber()) {
							(*mden)[i2].number().negate();
						} else if((*mden)[i2].isMultiplication() && (*mden)[i2].size() > 0) {
							if((*mden)[i2][0].isNumber()) {
								if((*mden)[i2][0].number().isMinusOne() && (*mden)[i2].size() > 1) {
									if((*mden)[i2].size() == 2) (*mden)[i2].setToChild(2, true);
									else (*mden)[i2].delChild(1);
								} else (*mden)[i2][0].number().negate();
							} else {
								(*mden)[i2].insertChild(m_minus_one, 1);
							}
						} else {
							(*mden)[i2].negate();
						}
					}
					mden->sort(po, false);
					if(mstruct[0].number().isMinusOne()) {
						if(mstruct.size() == 2) mstruct.setToChild(2, true);
						else mstruct.delChild(1);
					} else {
						mstruct[0].number().negate();
					}
					return true;
				}
			}
		}
	}
	bool b = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(unnegate_multiplier(mstruct[i], po)) {
			b = true;
		}
	}
	if(b) {
		mstruct.sort(po, false);
		return true;
	}
	return false;
}
void MathStructure::format(const PrintOptions &po) {
	if(!po.preserve_format) {
		if(po.place_units_separately) {
			factorizeUnits();
		}
		sort(po);
		setPrefixes(po);
		unnegate_multiplier(*this, po);
		if(po.improve_division_multipliers) {
			if(improve_division_multipliers(po)) sort(po);
		}
		remove_multi_one(*this);
	}
	formatsub(po, NULL, 0, true, this);
	if(!po.preserve_format) {
		postFormatUnits(po);
		if(po.sort_options.prefix_currencies && po.abbreviate_names && CALCULATOR->place_currency_code_before) {
			prefixCurrencies();
		}
	}
}
void MathStructure::formatsub(const PrintOptions &po, MathStructure *parent, size_t pindex, bool recursive, MathStructure *top_parent) {

	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CALCULATOR->aborted()) break;
			if(i == 1 && m_type == STRUCT_POWER && po.number_fraction_format < FRACTION_FRACTIONAL && CHILD(1).isNumber() && CHILD(1).number().isRational() && !CHILD(1).number().isInteger() && CHILD(1).number().numeratorIsLessThan(10) && CHILD(1).number().numeratorIsGreaterThan(-10) && CHILD(1).number().denominatorIsLessThan(10)) {
				PrintOptions po2 = po;
				po2.number_fraction_format = FRACTION_FRACTIONAL;
				CHILD(i).formatsub(po2, this, i + 1, false, top_parent);
			} else {
				CHILD(i).formatsub(po, this, i + 1, true, top_parent);
			}
			CHILD_UPDATED(i);
		}
	}
	switch(m_type) {
		case STRUCT_ADDITION: {
			break;
		}
		case STRUCT_NEGATE: {
			break;
		}
		case STRUCT_DIVISION: {
			if(po.preserve_format) break;
			if(CHILD(0).isAddition() && CHILD(0).size() > 0 && CHILD(0)[0].isNegate()) {
				int imin = 1;
				for(size_t i = 1; i < CHILD(0).size(); i++) {
					if(CHILD(0)[i].isNegate()) {
						imin++;
					} else {
						imin--;
					}
				}
				bool b = CHILD(1).isAddition() && CHILD(1).size() > 0 && CHILD(1)[0].isNegate();
				if(b) {
					imin++;
					for(size_t i = 1; i < CHILD(1).size(); i++) {
						if(CHILD(1)[i].isNegate()) {
							imin++;
						} else {
							imin--;
						}
					}
				}
				if(imin > 0 || (imin == 0 && parent && parent->isNegate())) {
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CHILD(0)[i].isNegate()) {
							CHILD(0)[i].setToChild(1, true);
						} else {
							CHILD(0)[i].transform(STRUCT_NEGATE);
						}
					}
					if(b) {
						for(size_t i = 0; i < CHILD(1).size(); i++) {
							if(CHILD(1)[i].isNegate()) {
								CHILD(1)[i].setToChild(1, true);
							} else {
								CHILD(1)[i].transform(STRUCT_NEGATE);
							}
						}
					} else {
						transform(STRUCT_NEGATE);
					}
					break;
				}
			} else if(CHILD(1).isAddition() && CHILD(1).size() > 0 && CHILD(1)[0].isNegate()) {
				int imin = 1;
				for(size_t i = 1; i < CHILD(1).size(); i++) {
					if(CHILD(1)[i].isNegate()) {
						imin++;
					} else {
						imin--;
					}
				}
				if(imin > 0 || (imin == 0 && parent && parent->isNegate())) {
					for(size_t i = 0; i < CHILD(1).size(); i++) {
						if(CHILD(1)[i].isNegate()) {
							CHILD(1)[i].setToChild(1, true);
						} else {
							CHILD(1)[i].transform(STRUCT_NEGATE);
						}
					}
					transform(STRUCT_NEGATE);
				}
			}
			break;
		}
		case STRUCT_INVERSE: {
			if(po.preserve_format) break;
			if((!parent || !parent->isMultiplication()) && CHILD(0).isAddition() && CHILD(0).size() > 0 && CHILD(0)[0].isNegate()) {
				int imin = 1;
				for(size_t i = 1; i < CHILD(0).size(); i++) {
					if(CHILD(0)[i].isNegate()) {
						imin++;
					} else {
						imin--;
					}
				}
				if(imin > 0 || (imin == 0 && parent && parent->isNegate())) {
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CHILD(0)[i].isNegate()) {
							CHILD(0)[i].setToChild(1, true);
						} else {
							CHILD(0)[i].transform(STRUCT_NEGATE);
						}
					}
					transform(STRUCT_NEGATE);
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(po.preserve_format) break;
			if(CHILD(0).isNegate()) {
				if(CHILD(0)[0].isOne()) {
					ERASE(0);
					if(SIZE == 1) {
						setToChild(1, true);
					}
				} else {
					CHILD(0).setToChild(1, true);
				}
				transform(STRUCT_NEGATE);
				CHILD(0).formatsub(po, this, 1, false, top_parent);
				break;
			}
			
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) break;
				if(CHILD(i).isInverse()) {
					if(!po.negative_exponents || !CHILD(i)[0].isNumber()) {
						b = true;
						break;
					}
				} else if(CHILD(i).isDivision()) {
					if(!CHILD(i)[0].isNumber() || !CHILD(i)[1].isNumber() || (!po.negative_exponents && CHILD(i)[0].number().isOne())) {
						b = true;
						break;
					}
				}
			}

			if(b) {
				MathStructure *den = new MathStructure();
				MathStructure *num = new MathStructure();
				num->setUndefined();
				short ds = 0, ns = 0;
				MathStructure *mnum = NULL, *mden = NULL;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isInverse()) {
						mden = &CHILD(i)[0];
					} else if(CHILD(i).isDivision()) {
						mnum = &CHILD(i)[0];
						mden = &CHILD(i)[1];
					} else {
						mnum = &CHILD(i);
					}
					if(mnum && !mnum->isOne()) {
						if(ns > 0) {
							if(mnum->isMultiplication() && num->isNumber()) {
								for(size_t i2 = 0; i2 < mnum->size(); i2++) {
									num->multiply((*mnum)[i2], true);
								}
							} else {
								num->multiply(*mnum, ns > 1);
							}
						} else {
							num->set(*mnum);
						}
						ns++;
						mnum = NULL;
					}
					if(mden) {
						if(ds > 0) {	
							if(mden->isMultiplication() && den->isNumber()) {
								for(size_t i2 = 0; i2 < mden->size(); i2++) {
									den->multiply((*mden)[i2], true);
								}
							} else {
								den->multiply(*mden, ds > 1);
							}							
						} else {
							den->set(*mden);
						}
						ds++;
						mden = NULL;
					}
				}
				clear(true);
				m_type = STRUCT_DIVISION;
				if(num->isUndefined()) num->set(m_one);
				APPEND_POINTER(num);
				APPEND_POINTER(den);
				num->formatsub(po, this, 1, false, top_parent);
				den->formatsub(po, this, 2, false, top_parent);
				formatsub(po, parent, pindex, false, top_parent);
				break;
			}

			size_t index = 0;
			if(CHILD(0).isOne()) {
				index = 1;
			}
			switch(CHILD(index).type()) {
				case STRUCT_POWER: {
					if(!CHILD(index)[0].isUnit_exp()) {
						break;
					}
				}
				case STRUCT_UNIT: {
					if(index == 0) {
						if(!parent || (!parent->isPower() && !parent->isMultiplication() && !parent->isInverse() && (!parent->isDivision() || pindex != 2))) {
							PREPEND(m_one);
						}
					}
					break;
				}
				default: {
					if(index == 1) {
						ERASE(0);
						if(SIZE == 1) {
							setToChild(1, true);
						}
					}
				}
			}
			break;
		}
		case STRUCT_UNIT: {
			if(po.preserve_format) break;
			if(!parent || (!parent->isPower() && !parent->isMultiplication() && !parent->isInverse() && !(parent->isDivision() && pindex == 2))) {
				multiply(m_one);
				SWAP_CHILDREN(0, 1);
			}
			break;
		}
		case STRUCT_POWER: {
			if(po.preserve_format) break;
			if(!po.negative_exponents && CHILD(1).isNegate() && (!CHILD(0).isVector() || !CHILD(1).isMinusOne())) {
				if(CHILD(1)[0].isOne()) {
					m_type = STRUCT_INVERSE;
					ERASE(1);
				} else {
					CHILD(1).setToChild(1, true);
					transform(STRUCT_INVERSE);
				}
				formatsub(po, parent, pindex, true, top_parent);
				break;
			} else if(po.halfexp_to_sqrt && ((CHILD(1).isDivision() && CHILD(1)[0].isNumber() && CHILD(1)[0].number().isInteger() && CHILD(1)[1].isNumber() && CHILD(1)[1].number().isTwo() && ((!po.negative_exponents && (CHILD(0).countChildren() == 0 || CHILD(0).isFunction())) || CHILD(1)[0].isOne())) || (CHILD(1).isNumber() && CHILD(1).number().denominatorIsTwo() && ((!po.negative_exponents && (CHILD(0).countChildren() == 0 || CHILD(0).isFunction())) || CHILD(1).number().numeratorIsOne())) || (CHILD(1).isInverse() && CHILD(1)[0].isNumber() && CHILD(1)[0].number() == 2))) {
				if(CHILD(1).isInverse() || (CHILD(1).isDivision() && CHILD(1)[0].number().isOne()) || (CHILD(1).isNumber() && CHILD(1).number().numeratorIsOne())) {
					m_type = STRUCT_FUNCTION;
					ERASE(1)
					setFunction(CALCULATOR->f_sqrt);
				} else {
					if(CHILD(1).isNumber()) {
						CHILD(1).number() -= nr_half;
					} else {
						Number nr = CHILD(1)[0].number();
						nr /= CHILD(1)[1].number();
						nr.floor();
						CHILD(1).set(nr);
					}
					if(CHILD(1).number().isOne()) {
						setToChild(1, true);
						if(parent && parent->isMultiplication()) {
							parent->insertChild(MathStructure(CALCULATOR->f_sqrt, this, NULL), pindex + 1);
						} else {
							multiply(MathStructure(CALCULATOR->f_sqrt, this, NULL));
						}
					} else {
						if(parent && parent->isMultiplication()) {
							parent->insertChild(MathStructure(CALCULATOR->f_sqrt, &CHILD(0), NULL), pindex + 1);
						} else {
							multiply(MathStructure(CALCULATOR->f_sqrt, &CHILD(0), NULL));
						}
					}
				}
				formatsub(po, parent, pindex, false, top_parent);
				break;
			} else if(po.exp_to_root && CHILD(0).representsNonNegative(true) && ((CHILD(1).isDivision() && CHILD(1)[0].isNumber() && CHILD(1)[0].number().isInteger() && CHILD(1)[1].isNumber() && CHILD(1)[1].number().isGreaterThan(1) && CHILD(1)[1].number().isLessThan(10) && ((!po.negative_exponents && (CHILD(0).countChildren() == 0 || CHILD(0).isFunction())) || CHILD(1)[0].isOne())) || (CHILD(1).isNumber() && CHILD(1).number().isRational() && !CHILD(1).number().isInteger() && CHILD(1).number().denominatorIsLessThan(10) && ((!po.negative_exponents && (CHILD(0).countChildren() == 0 || CHILD(0).isFunction())) || CHILD(1).number().numeratorIsOne())) || (CHILD(1).isInverse() && CHILD(1)[0].isNumber()  && CHILD(1)[0].number().isInteger() && CHILD(1)[0].number().isPositive() && CHILD(1)[0].number().isLessThan(10)))) {
				Number nr_int, nr_num, nr_den;
				if(CHILD(1).isNumber()) {
					nr_num = CHILD(1).number().numerator();
					nr_den = CHILD(1).number().denominator();
				} else if(CHILD(1).isDivision()) {
					nr_num.set(CHILD(1)[0].number());
					nr_den.set(CHILD(1)[1].number());
				} else if(CHILD(1).isInverse()) {
					nr_num.set(1, 1, 0);
					nr_den.set(CHILD(1)[0].number());
				}
				if(!nr_num.isOne() && (nr_num - 1).isIntegerDivisible(nr_den)) {
					nr_int = nr_num;
					nr_int--;
					nr_int.divide(nr_den);
					nr_num = 1;
				}
				MathStructure mbase(CHILD(0));
				CHILD(1) = nr_den;
				m_type = STRUCT_FUNCTION;
				setFunction(CALCULATOR->f_root);
				formatsub(po, parent, pindex, false, top_parent);
				if(!nr_num.isOne()) {
					raise(nr_num);
					formatsub(po, parent, pindex, false, top_parent);
				}
				if(!nr_int.isZero()) {
					if(!nr_int.isOne()) mbase.raise(nr_int);
					multiply(mbase);
					sort(po);
					formatsub(po, parent, pindex, false, top_parent);
				}
				break;
			}
			if(CHILD(0).isUnit_exp() && (!parent || (!parent->isPower() && !parent->isMultiplication() && !parent->isInverse() && !(parent->isDivision() && pindex == 2)))) {
				multiply(m_one);
				SWAP_CHILDREN(0, 1);
			}
			break;
		}
		case STRUCT_FUNCTION: {
			if(po.preserve_format) break;
			if(o_function == CALCULATOR->f_root && SIZE == 2 && CHILD(1) == 3) {
				ERASE(1)
				setFunction(CALCULATOR->f_cbrt);
			}
			break;
		}
		case STRUCT_VARIABLE: {
			if(o_variable == CALCULATOR->v_pinf || o_variable == CALCULATOR->v_minf) {
				set(((KnownVariable*) o_variable)->get());
			}
			break;
		}
		case STRUCT_NUMBER: {
			if(o_number.isNegative() || ((parent || po.interval_display != INTERVAL_DISPLAY_SIGNIFICANT_DIGITS) && o_number.isInterval() && o_number.isNonPositive())) {
				if(!o_number.isMinusInfinity() || (parent && parent->isAddition())) {
					o_number.negate();
					transform(STRUCT_NEGATE);
					formatsub(po, parent, pindex, true, top_parent);
				}
			} else if(po.number_fraction_format == FRACTION_COMBINED && po.base != BASE_SEXAGESIMAL && po.base != BASE_TIME && o_number.isRational() && !o_number.isInteger()) {
				if(o_number.isFraction()) {
					Number num(o_number.numerator());
					Number den(o_number.denominator());
					clear(true);
					if(num.isOne()) {
						m_type = STRUCT_INVERSE;
					} else {
						m_type = STRUCT_DIVISION;
						APPEND_NEW(num);
					}
					APPEND_NEW(den);
				} else {
					Number frac(o_number);
					frac.frac();
					MathStructure *num = new MathStructure(frac.numerator());
					num->transform(STRUCT_DIVISION, frac.denominator());
					o_number.trunc();
					add_nocopy(num);
				}
			} else if((po.number_fraction_format == FRACTION_FRACTIONAL || po.base == BASE_ROMAN_NUMERALS || po.number_fraction_format == FRACTION_DECIMAL_EXACT) && po.base != BASE_SEXAGESIMAL && po.base != BASE_TIME && o_number.isRational() && !o_number.isInteger() && !o_number.isApproximate()) {
				string str_den = "";
				InternalPrintStruct ips_n;
				if(isApproximate() || (top_parent && top_parent->isApproximate())) ips_n.parent_approximate = true;
				ips_n.parent_precision = precision();
				if(top_parent && top_parent->precision() < 0 && top_parent->precision() < ips_n.parent_precision) ips_n.parent_precision = top_parent->precision();
				ips_n.den = &str_den;
				PrintOptions po2 = po;
				po2.is_approximate = NULL;
				o_number.print(po2, ips_n);
				if(!str_den.empty()) {
					Number num(o_number.numerator());
					Number den(o_number.denominator());
					clear(true);
					if(num.isOne()) {
						m_type = STRUCT_INVERSE;
					} else {
						m_type = STRUCT_DIVISION;
						APPEND_NEW(num);
					}
					APPEND_NEW(den);
				}
			} else if(o_number.hasImaginaryPart()) {
				if(o_number.hasRealPart()) {
					Number re(o_number.realPart());
					Number im(o_number.imaginaryPart());
					MathStructure *mstruct = new MathStructure(im);
					if(im.isOne()) {
						mstruct->set(CALCULATOR->v_i);
					} else {
						mstruct->multiply_nocopy(new MathStructure(CALCULATOR->v_i));
					}
					o_number = re;
					add_nocopy(mstruct);
					formatsub(po, parent, pindex, true, top_parent);
				} else {
					Number im(o_number.imaginaryPart());
					if(im.isOne()) {
						set(CALCULATOR->v_i, true);
					} else if(im.isMinusOne()) {
						set(CALCULATOR->v_i, true);
						transform(STRUCT_NEGATE);
					} else {
						o_number = im;
						multiply_nocopy(new MathStructure(CALCULATOR->v_i));
					}
					formatsub(po, parent, pindex, true, top_parent);
				}
			}
			break;
		}
		default: {}
	}
}

int namelen(const MathStructure &mstruct, const PrintOptions &po, const InternalPrintStruct&, bool *abbreviated = NULL) {
	const string *str;
	switch(mstruct.type()) {
		case STRUCT_FUNCTION: {
			const ExpressionName *ename = &mstruct.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		case STRUCT_VARIABLE:  {
			const ExpressionName *ename = &mstruct.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		case STRUCT_ABORTED: {}
		case STRUCT_SYMBOLIC:  {
			str = &mstruct.symbol();
			if(abbreviated) *abbreviated = false;
			break;
		}
		case STRUCT_UNIT:  {
			const ExpressionName *ename = &mstruct.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct.isPlural(), po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		default: {if(abbreviated) *abbreviated = false; return 0;}
	}
	if(text_length_is_one(*str)) return 1;
	return str->length();
}

bool MathStructure::needsParenthesis(const PrintOptions &po, const InternalPrintStruct &ips, const MathStructure &parent, size_t index, bool flat_division, bool) const {
	switch(parent.type()) {
		case STRUCT_MULTIPLICATION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return flat_division;}
				case STRUCT_INVERSE: {return flat_division;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return po.excessive_parenthesis;}
				default: {return true;}
			}
		}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_POWER: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_OR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_XOR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_NOT: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_OR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_XOR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_NOT: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return (flat_division || po.excessive_parenthesis) && o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_ADDITION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return index > 1 || po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return true;}
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_POWER: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return index == 1 || flat_division || po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return index == 1 || flat_division || po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return true;}
				case STRUCT_NEGATE: {return index == 1 || CHILD(0).needsParenthesis(po, ips, parent, index, flat_division);}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return index == 1 || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return index == 1 || po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_NEGATE: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return true;}
				case STRUCT_NEGATE: {return true;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_XOR: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return flat_division;}
				case STRUCT_INVERSE: {return flat_division;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return false;}
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return po.excessive_parenthesis && o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_XOR: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return flat_division;}
				case STRUCT_INVERSE: {return flat_division;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return true;}
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return po.excessive_parenthesis && o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_COMPARISON: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return po.excessive_parenthesis;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return true;}
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return po.excessive_parenthesis && o_number.isInfinite();}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_ABORTED: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_LOGICAL_NOT: {}
		case STRUCT_BITWISE_NOT: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return true;}
				case STRUCT_INVERSE: {return true;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return true;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return true;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return po.excessive_parenthesis;}
				case STRUCT_VECTOR: {return po.excessive_parenthesis;}
				case STRUCT_NUMBER: {return po.excessive_parenthesis;}
				case STRUCT_VARIABLE: {return po.excessive_parenthesis;}
				case STRUCT_ABORTED: {return po.excessive_parenthesis;}
				case STRUCT_SYMBOLIC: {return po.excessive_parenthesis;}
				case STRUCT_UNIT: {return po.excessive_parenthesis;}
				case STRUCT_UNDEFINED: {return po.excessive_parenthesis;}
				default: {return true;}
			}
		}
		case STRUCT_FUNCTION: {
			return false;
		}
		case STRUCT_VECTOR: {
			return false;
		}
		default: {
			return true;
		}
	}
}

int MathStructure::neededMultiplicationSign(const PrintOptions &po, const InternalPrintStruct &ips, const MathStructure &parent, size_t index, bool par, bool par_prev, bool flat_division, bool flat_power) const {
	if(!po.short_multiplication) return MULTIPLICATION_SIGN_OPERATOR;
	if(index <= 1) return MULTIPLICATION_SIGN_NONE;
	if(par_prev && par) return MULTIPLICATION_SIGN_NONE;
	if(par_prev) {
		if(isUnit_exp()) return MULTIPLICATION_SIGN_SPACE;
		if(isUnknown_exp()) {
			if(isSymbolic() || (isPower() && CHILD(0).isSymbolic())) return MULTIPLICATION_SIGN_SPACE;
			return (namelen(isPower() ? CHILD(0) : *this, po, ips, NULL) > 1 ? MULTIPLICATION_SIGN_SPACE : MULTIPLICATION_SIGN_NONE);
		}
		if(isMultiplication() && SIZE > 0) {
			if(CHILD(0).isUnit_exp()) return MULTIPLICATION_SIGN_SPACE;
			if(CHILD(0).isUnknown_exp()) {
				if(CHILD(0).isSymbolic() || (CHILD(0).isPower() && CHILD(0)[0].isSymbolic())) return MULTIPLICATION_SIGN_SPACE;
				return (namelen(CHILD(0).isPower() ? CHILD(0)[0] : CHILD(0), po, ips, NULL) > 1 ? MULTIPLICATION_SIGN_SPACE : MULTIPLICATION_SIGN_NONE);
			}
		} else if(isDivision()) {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).isUnit_exp()) {
					return MULTIPLICATION_SIGN_OPERATOR;
				}
			}
			return MULTIPLICATION_SIGN_SPACE;
		}
		return MULTIPLICATION_SIGN_OPERATOR;
	}
	int t = parent[index - 2].type();
	if(flat_power && t == STRUCT_POWER) return MULTIPLICATION_SIGN_OPERATOR;
	if(par && t == STRUCT_POWER) return MULTIPLICATION_SIGN_SPACE;
	if(par) return MULTIPLICATION_SIGN_NONE;
	bool abbr_prev = false, abbr_this = false;
	int namelen_this = namelen(*this, po, ips, &abbr_this);
	int namelen_prev = namelen(parent[index - 2], po, ips, &abbr_prev);
	switch(t) {
		case STRUCT_MULTIPLICATION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {if(flat_division) return MULTIPLICATION_SIGN_OPERATOR; return MULTIPLICATION_SIGN_SPACE;}
		case STRUCT_ADDITION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_POWER: {if(flat_power) return MULTIPLICATION_SIGN_OPERATOR; break;}
		case STRUCT_NEGATE: {break;}
		case STRUCT_BITWISE_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_COMPARISON: {return MULTIPLICATION_SIGN_OPERATOR;}		
		case STRUCT_FUNCTION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_VECTOR: {break;}
		case STRUCT_NUMBER: {break;}
		case STRUCT_VARIABLE: {break;}
		case STRUCT_ABORTED: {break;}
		case STRUCT_SYMBOLIC: {break;}
		case STRUCT_UNIT: {
			if(m_type == STRUCT_UNIT) {
				if(!po.limit_implicit_multiplication && !abbr_prev && !abbr_this) {
					return MULTIPLICATION_SIGN_SPACE;
				} 
				if(po.place_units_separately) {
					return MULTIPLICATION_SIGN_OPERATOR_SHORT;
				} else {
					return MULTIPLICATION_SIGN_OPERATOR;
				}
			} else if(m_type == STRUCT_NUMBER) {
				if(namelen_prev > 1) {
					return MULTIPLICATION_SIGN_SPACE;
				} else {
					return MULTIPLICATION_SIGN_NONE;
				}
			}
			//return MULTIPLICATION_SIGN_SPACE;
		}
		case STRUCT_UNDEFINED: {break;}
		default: {return MULTIPLICATION_SIGN_OPERATOR;}
	}
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {if(flat_division) return MULTIPLICATION_SIGN_OPERATOR; return MULTIPLICATION_SIGN_SPACE;}
		case STRUCT_ADDITION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_POWER: {return CHILD(0).neededMultiplicationSign(po, ips, parent, index, par, par_prev, flat_division, flat_power);}
		case STRUCT_NEGATE: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_COMPARISON: {return MULTIPLICATION_SIGN_OPERATOR;}		
		case STRUCT_FUNCTION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_VECTOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_NUMBER: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_ABORTED: {}
		case STRUCT_VARIABLE: {}
		case STRUCT_SYMBOLIC: {
			if(po.limit_implicit_multiplication && t != STRUCT_NUMBER) return MULTIPLICATION_SIGN_OPERATOR;
			if(t != STRUCT_NUMBER && ((namelen_prev > 1 || namelen_this > 1) || equals(parent[index - 2]))) return MULTIPLICATION_SIGN_OPERATOR;
			if(namelen_this > 1 || (m_type == STRUCT_SYMBOLIC && !po.allow_non_usable)) return MULTIPLICATION_SIGN_SPACE;
			return MULTIPLICATION_SIGN_NONE;
		}
		case STRUCT_UNIT: {
			if(t == STRUCT_POWER && parent[index - 2][0].isUnit_exp()) {
				return MULTIPLICATION_SIGN_NONE;
			}			
			return MULTIPLICATION_SIGN_SPACE;
		}
		case STRUCT_UNDEFINED: {return MULTIPLICATION_SIGN_OPERATOR;}
		default: {return MULTIPLICATION_SIGN_OPERATOR;}
	}
}

ostream& operator << (ostream &os, const MathStructure &mstruct) {
	os << mstruct.print();
	return os;
}
string MathStructure::print(const PrintOptions &po, const InternalPrintStruct &ips) const {
	if(ips.depth == 0 && po.is_approximate) *po.is_approximate = false;
	string print_str;
	InternalPrintStruct ips_n = ips;
	if(isApproximate()) ips_n.parent_approximate = true;
	if(precision() >= 0 && (ips_n.parent_precision < 0 || precision() < ips_n.parent_precision)) ips_n.parent_precision = precision();
	switch(m_type) {
		case STRUCT_NUMBER: {
			print_str = o_number.print(po, ips_n);
			break;
		}
		case STRUCT_ABORTED: {}
		case STRUCT_SYMBOLIC: {
			if(po.allow_non_usable) {
				print_str = s_sym;
			} else {
				print_str = "\"";
				print_str += s_sym;
				print_str += "\"";
			}
			break;
		}
		case STRUCT_ADDITION: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(CHILD(i).type() == STRUCT_NEGATE) {
						if(po.spacious) print_str += " ";
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MINUS, po.can_display_unicode_string_arg))) print_str += SIGN_MINUS;
						else print_str += "-";
						if(po.spacious) print_str += " ";
						ips_n.wrap = CHILD(i)[0].needsParenthesis(po, ips_n, *this, i + 1, true, true);
						print_str += CHILD(i)[0].print(po, ips_n);
					} else {
						if(po.spacious) print_str += " ";
						print_str += "+";
						if(po.spacious) print_str += " ";
						ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
						print_str += CHILD(i).print(po, ips_n);
					}
				} else {
					ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
					print_str += CHILD(i).print(po, ips_n);
				}
			}
			break;
		}
		case STRUCT_NEGATE: {
			if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MINUS, po.can_display_unicode_string_arg))) print_str += SIGN_MINUS;
			else print_str = "-";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_MULTIPLICATION: {
			ips_n.depth++;
			bool par_prev = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				if(!po.short_multiplication && i > 0) {
					if(po.spacious) print_str += " ";
					if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIDOT;
					else if(po.use_unicode_signs && (po.multiplication_sign == MULTIPLICATION_SIGN_DOT || po.multiplication_sign == MULTIPLICATION_SIGN_ALTDOT) && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MIDDLEDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MIDDLEDOT;
					else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIPLICATION;
					else print_str += "*";
					if(po.spacious) print_str += " ";
				} else if(i > 0) {
					switch(CHILD(i).neededMultiplicationSign(po, ips_n, *this, i + 1, ips_n.wrap || (CHILD(i).isPower() && CHILD(i)[0].needsParenthesis(po, ips_n, CHILD(i), 1, true, true)), par_prev, true, true)) {
						case MULTIPLICATION_SIGN_SPACE: {print_str += " "; break;}
						case MULTIPLICATION_SIGN_OPERATOR: {
							if(po.spacious) {
								if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += " " SIGN_MULTIDOT " ";
								else if(po.use_unicode_signs && (po.multiplication_sign == MULTIPLICATION_SIGN_DOT || po.multiplication_sign == MULTIPLICATION_SIGN_ALTDOT) && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MIDDLEDOT, po.can_display_unicode_string_arg))) print_str += " " SIGN_MIDDLEDOT " ";
								else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += " " SIGN_MULTIPLICATION " ";
								else print_str += " * ";
								break;
							}
						}
						case MULTIPLICATION_SIGN_OPERATOR_SHORT: {
							if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIDOT;
							else if(po.use_unicode_signs && (po.multiplication_sign == MULTIPLICATION_SIGN_DOT || po.multiplication_sign == MULTIPLICATION_SIGN_ALTDOT) && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MIDDLEDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MIDDLEDOT;
							else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIPLICATION;
							else print_str += "*";
							break;
						}
					}
				}
				print_str += CHILD(i).print(po, ips_n);
				par_prev = ips_n.wrap;
			}
			break;
		}
		case STRUCT_INVERSE: {
			print_str = "1";
			if(po.spacious) print_str += " ";
			if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION;
			} else if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION_SLASH && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION_SLASH, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION_SLASH;
			} else {
				print_str += "/";
			}
			if(po.spacious) print_str += " ";
			ips_n.depth++;
			ips_n.division_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_DIVISION: {
			ips_n.depth++;
			ips_n.division_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			if(po.spacious) print_str += " ";
			if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION;
			} else if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION_SLASH && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION_SLASH, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION_SLASH;
			} else {
				print_str += "/";
			}
			if(po.spacious) print_str += " ";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			print_str += CHILD(1).print(po, ips_n);
			break;
		}
		case STRUCT_POWER: {
			ips_n.depth++;
			ips_n.power_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			print_str += "^";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			PrintOptions po2 = po;
			po2.show_ending_zeroes = false;
			print_str += CHILD(1).print(po2, ips_n);
			break;
		}
		case STRUCT_COMPARISON: {
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			if(po.spacious) print_str += " ";
			switch(ct_comp) {
				case COMPARISON_EQUALS: {
					if(po.use_unicode_signs && po.interval_display != INTERVAL_DISPLAY_INTERVAL && isApproximate() && containsInterval() && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_ALMOST_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_ALMOST_EQUAL;
					else print_str += "="; 
					break;
				}
				case COMPARISON_NOT_EQUALS: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_NOT_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_NOT_EQUAL;
					else print_str += "!="; 
					break;
				}
				case COMPARISON_GREATER: {print_str += ">"; break;}
				case COMPARISON_LESS: {print_str += "<"; break;}
				case COMPARISON_EQUALS_GREATER: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_GREATER_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_GREATER_OR_EQUAL;
					else print_str += ">="; 
					break;
				}
				case COMPARISON_EQUALS_LESS: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_LESS_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_LESS_OR_EQUAL;
					else print_str += "<="; 
					break;
				}
			}
			if(po.spacious) print_str += " ";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			print_str += CHILD(1).print(po, ips_n);
			break;
		}
		case STRUCT_BITWISE_AND: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "&";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_OR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "|";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_XOR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "XOR";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_NOT: {
			print_str = "~";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_LOGICAL_AND: {
			ips_n.depth++;
			if(!po.preserve_format && SIZE == 2 && CHILD(0).isComparison() && CHILD(1).isComparison() && CHILD(0).comparisonType() != COMPARISON_EQUALS && CHILD(0).comparisonType() != COMPARISON_NOT_EQUALS && CHILD(1).comparisonType() != COMPARISON_EQUALS && CHILD(1).comparisonType() != COMPARISON_NOT_EQUALS && CHILD(0)[0] == CHILD(1)[0]) {
				ips_n.wrap = CHILD(0)[1].needsParenthesis(po, ips_n, CHILD(0), 2, true, true);
				print_str += CHILD(0)[1].print(po, ips_n);
				if(po.spacious) print_str += " ";
				switch(CHILD(0).comparisonType()) {
					case COMPARISON_LESS: {print_str += ">"; break;}
					case COMPARISON_GREATER: {print_str += "<"; break;}
					case COMPARISON_EQUALS_LESS: {
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_GREATER_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_GREATER_OR_EQUAL;
						else print_str += ">="; 
						break;
					}
					case COMPARISON_EQUALS_GREATER: {
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_LESS_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_LESS_OR_EQUAL;
						else print_str += "<="; 
						break;
					}
					default: {}
				}
				if(po.spacious) print_str += " ";
				
				ips_n.wrap = CHILD(0)[0].needsParenthesis(po, ips_n, CHILD(0), 1, true, true);
				print_str += CHILD(0)[0].print(po, ips_n);
				
				if(po.spacious) print_str += " ";
				switch(CHILD(1).comparisonType()) {
					case COMPARISON_GREATER: {print_str += ">"; break;}
					case COMPARISON_LESS: {print_str += "<"; break;}
					case COMPARISON_EQUALS_GREATER: {
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_GREATER_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_GREATER_OR_EQUAL;
						else print_str += ">="; 
						break;
					}
					case COMPARISON_EQUALS_LESS: {
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_LESS_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_LESS_OR_EQUAL;
						else print_str += "<="; 
						break;
					}
					default: {}
				}
				if(po.spacious) print_str += " ";
				
				ips_n.wrap = CHILD(1)[1].needsParenthesis(po, ips_n, CHILD(1), 2, true, true);
				print_str += CHILD(1)[1].print(po, ips_n);
				
				break;
			}
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					if(po.spell_out_logical_operators) print_str += _("and");
					else print_str += "&&";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_OR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					if(po.spell_out_logical_operators) print_str += _("or");
					else print_str += "||";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_XOR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "XOR";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_NOT: {
			print_str = "!";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_VECTOR: {
			ips_n.depth++;
			print_str = "[";
			for(size_t i = 0; i < SIZE; i++) {
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					print_str += po.comma();
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			print_str += "]";
			break;
		}
		case STRUCT_UNIT: {
			const ExpressionName *ename = &o_unit->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, b_plural, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			if(o_prefix) print_str += o_prefix->name(po.abbreviate_names && ename->abbreviation, po.use_unicode_signs, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			break;
		}
		case STRUCT_VARIABLE: {
			const ExpressionName *ename = &o_variable->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			break;
		}
		case STRUCT_FUNCTION: {
			ips_n.depth++;
			const ExpressionName *ename = &o_function->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			print_str += "(";
			for(size_t i = 0; i < SIZE; i++) {
				if(i == 1 && o_function == CALCULATOR->f_signum && CHILD(1).isZero()) break;
				if(CALCULATOR->aborted()) return CALCULATOR->abortedMessage();
				if(i > 0) {
					print_str += po.comma();
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			print_str += ")";
			break;
		}
		case STRUCT_UNDEFINED: {
			print_str = _("undefined");
			break;
		}
	}
	if(CALCULATOR->aborted()) print_str = CALCULATOR->abortedMessage();
	if(ips.wrap) {
		print_str.insert(0, "(");
		print_str += ")";
	}	
	return print_str;
}

MathStructure &MathStructure::flattenVector(MathStructure &mstruct) const {
	if(!isVector()) {
		mstruct = *this;
		return mstruct;
	}
	MathStructure mstruct2;
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			CHILD(i).flattenVector(mstruct2);
			for(size_t i2 = 0; i2 < mstruct2.size(); i2++) {
				mstruct.addChild(mstruct2[i2]);
			}
		} else {
			mstruct.addChild(CHILD(i));
		}
	}
	return mstruct;
}
bool MathStructure::rankVector(bool ascending) {
	vector<int> ranked;
	vector<bool> ranked_equals_prev;	
	bool b;
	for(size_t index = 0; index < SIZE; index++) {
		b = false;
		for(size_t i = 0; i < ranked.size(); i++) {
			if(CALCULATOR->aborted()) return false;
			ComparisonResult cmp = CHILD(index).compare(CHILD(ranked[i]));
			if(COMPARISON_NOT_FULLY_KNOWN(cmp)) {
				CALCULATOR->error(true, _("Unsolvable comparison at element %s when trying to rank vector."), i2s(index).c_str(), NULL);
				return false;
			}
			if((ascending && cmp == COMPARISON_RESULT_GREATER) || cmp == COMPARISON_RESULT_EQUAL || (!ascending && cmp == COMPARISON_RESULT_LESS)) {
				if(cmp == COMPARISON_RESULT_EQUAL) {
					ranked.insert(ranked.begin() + i + 1, index);
					ranked_equals_prev.insert(ranked_equals_prev.begin() + i + 1, true);
				} else {
					ranked.insert(ranked.begin() + i, index);
					ranked_equals_prev.insert(ranked_equals_prev.begin() + i, false);
				}
				b = true;
				break;
			}
		}
		if(!b) {
			ranked.push_back(index);
			ranked_equals_prev.push_back(false);
		}
	}	
	int n_rep = 0;
	for(long int i = (long int) ranked.size() - 1; i >= 0; i--) {
		if(CALCULATOR->aborted()) return false;
		if(ranked_equals_prev[i]) {
			n_rep++;
		} else {
			if(n_rep) {
				MathStructure v(i + 1 + n_rep, 1L, 0L);
				v += i + 1;
				v *= MathStructure(1, 2, 0);
				for(; n_rep >= 0; n_rep--) {
					CHILD(ranked[i + n_rep]) = v;
				}
			} else {
				CHILD(ranked[i]).set(i + 1, 1L, 0L);
			}
			n_rep = 0;
		}
	}
	return true;
}
bool MathStructure::sortVector(bool ascending) {
	vector<size_t> ranked_mstructs;
	bool b;
	for(size_t index = 0; index < SIZE; index++) {
		b = false;
		for(size_t i = 0; i < ranked_mstructs.size(); i++) {
			if(CALCULATOR->aborted()) return false;
			ComparisonResult cmp = CHILD(index).compare(*v_subs[ranked_mstructs[i]]);
			if(COMPARISON_MIGHT_BE_LESS_OR_GREATER(cmp)) {
				CALCULATOR->error(true, _("Unsolvable comparison at element %s when trying to sort vector."), i2s(index).c_str(), NULL);
				return false;
			}
			if((ascending && COMPARISON_IS_EQUAL_OR_GREATER(cmp)) || (!ascending && COMPARISON_IS_EQUAL_OR_LESS(cmp))) {
				ranked_mstructs.insert(ranked_mstructs.begin() + i, v_order[index]);
				b = true;
				break;
			}
		}
		if(!b) {
			ranked_mstructs.push_back(v_order[index]);
		}
	}	
	v_order = ranked_mstructs;
	return true;
}
MathStructure &MathStructure::getRange(int start, int end, MathStructure &mstruct) const {
	if(!isVector()) {
		if(start > 1) {
			mstruct.clear();
			return mstruct;
		} else {
			mstruct = *this;
			return mstruct;
		}
	}
	if(start < 1) start = 1;
	else if(start > (long int) SIZE) {
		mstruct.clear();
		return mstruct;
	}
	if(end < (int) 1 || end > (long int) SIZE) end = SIZE;
	else if(end < start) end = start;	
	mstruct.clearVector();
	for(; start <= end; start++) {
		mstruct.addChild(CHILD(start - 1));
	}
	return mstruct;
}

void MathStructure::resizeVector(size_t i, const MathStructure &mfill) {
	if(i > SIZE) {
		while(i > SIZE) {
			APPEND(mfill);
		}
	} else if(i < SIZE) {
		REDUCE(i)
	}
}

size_t MathStructure::rows() const {
	if(m_type != STRUCT_VECTOR || SIZE == 0 || (SIZE == 1 && (!CHILD(0).isVector() || CHILD(0).size() == 0))) return 0;
	return SIZE;
}
size_t MathStructure::columns() const {
	if(m_type != STRUCT_VECTOR || SIZE == 0 || !CHILD(0).isVector()) return 0;
	return CHILD(0).size();
}
const MathStructure *MathStructure::getElement(size_t row, size_t column) const {
	if(row == 0 || column == 0 || row > rows() || column > columns()) return NULL;
	if(CHILD(row - 1).size() < column) return NULL;
	return &CHILD(row - 1)[column - 1];
}
MathStructure *MathStructure::getElement(size_t row, size_t column) {
	if(row == 0 || column == 0 || row > rows() || column > columns()) return NULL;
	if(CHILD(row - 1).size() < column) return NULL;
	return &CHILD(row - 1)[column - 1];
}
MathStructure &MathStructure::getArea(size_t r1, size_t c1, size_t r2, size_t c2, MathStructure &mstruct) const {
	size_t r = rows();
	size_t c = columns();
	if(r1 < 1) r1 = 1;
	else if(r1 > r) r1 = r;
	if(c1 < 1) c1 = 1;
	else if(c1 > c) c1 = c;
	if(r2 < 1 || r2 > r) r2 = r;
	else if(r2 < r1) r2 = r1;
	if(c2 < 1 || c2 > c) c2 = c;
	else if(c2 < c1) c2 = c1;
	mstruct.clearMatrix(); mstruct.resizeMatrix(r2 - r1 + 1, c2 - c1 + 1, m_undefined);
	for(size_t index_r = r1; index_r <= r2; index_r++) {
		for(size_t index_c = c1; index_c <= c2; index_c++) {
			mstruct[index_r - r1][index_c - c1] = CHILD(index_r - 1)[index_c - 1];
		}			
	}
	return mstruct;
}
MathStructure &MathStructure::rowToVector(size_t r, MathStructure &mstruct) const {
	if(r > rows()) {
		mstruct = m_undefined;
		return mstruct;
	}
	if(r < 1) r = 1;
	mstruct = CHILD(r - 1);
	return mstruct;
}
MathStructure &MathStructure::columnToVector(size_t c, MathStructure &mstruct) const {
	if(c > columns()) {
		mstruct = m_undefined;
		return mstruct;
	}
	if(c < 1) c = 1;
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		mstruct.addChild(CHILD(i)[c - 1]);
	}
	return mstruct;
}
MathStructure &MathStructure::matrixToVector(MathStructure &mstruct) const {
	if(!isVector()) {
		mstruct = *this;
		return mstruct;
	}
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
				mstruct.addChild(CHILD(i)[i2]);
			}
		} else {
			mstruct.addChild(CHILD(i));
		}
	}
	return mstruct;
}
void MathStructure::setElement(const MathStructure &mstruct, size_t row, size_t column) {
	if(row > rows() || column > columns() || row < 1 || column < 1) return;
	CHILD(row - 1)[column - 1] = mstruct;
	CHILD(row - 1).childUpdated(column);
	CHILD_UPDATED(row - 1);
}
void MathStructure::addRows(size_t r, const MathStructure &mfill) {
	if(r == 0) return;
	size_t cols = columns();
	MathStructure mstruct; mstruct.clearVector();
	mstruct.resizeVector(cols, mfill);
	for(size_t i = 0; i < r; i++) {
		APPEND(mstruct);
	}
}
void MathStructure::addColumns(size_t c, const MathStructure &mfill) {
	if(c == 0) return;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			for(size_t i2 = 0; i2 < c; i2++) {
				CHILD(i).addChild(mfill);
			}
		}
	}
	CHILDREN_UPDATED;
}
void MathStructure::addRow(const MathStructure &mfill) {
	addRows(1, mfill);
}
void MathStructure::addColumn(const MathStructure &mfill) {
	addColumns(1, mfill);
}
void MathStructure::resizeMatrix(size_t r, size_t c, const MathStructure &mfill) {
	if(r > SIZE) {
		addRows(r - SIZE, mfill);
	} else if(r != SIZE) {
		REDUCE(r);
	}
	size_t cols = columns();
	if(c > cols) {
		addColumns(c - cols, mfill);
	} else if(c != cols) {
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).resizeVector(c, mfill);
		}
	}
}
bool MathStructure::matrixIsSquare() const {
	return rows() == columns();
}

bool MathStructure::isNumericMatrix() const {
	if(!isMatrix()) return false;
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		for(size_t index_c = 0; index_c < CHILD(index_r).size(); index_c++) {
			if(!CHILD(index_r)[index_c].isNumber() || CHILD(index_r)[index_c].isInfinity()) return false;
		}
	}
	return true;
}

//from GiNaC
int MathStructure::pivot(size_t ro, size_t co, bool symbolic) {

	size_t k = ro;
	if(symbolic) {
		while((k < SIZE) && (CHILD(k)[co].isZero())) ++k;
	} else {
		size_t kmax = k + 1;
		Number mmax(CHILD(kmax)[co].number());
		mmax.abs();
		while(kmax < SIZE) {
			if(CHILD(kmax)[co].number().isNegative()) {
				Number ntmp(CHILD(kmax)[co].number());
				ntmp.negate();
				if(ntmp.isGreaterThan(mmax)) {
					mmax = ntmp;
					k = kmax;
				}
			} else if(CHILD(kmax)[co].number().isGreaterThan(mmax)) {
				mmax = CHILD(kmax)[co].number();
				k = kmax;
			}
			++kmax;
		}
		if(!mmax.isZero()) k = kmax;
	}
	if(k == SIZE) return -1;
	if(k == ro) return 0;
	
	SWAP_CHILDREN(ro, k)
	
	return k;
	
}


//from GiNaC
void determinant_minor(const MathStructure &mtrx, MathStructure &mdet, const EvaluationOptions &eo) {
	size_t n = mtrx.size();
	if(n == 1) {
		mdet = mtrx[0][0];
		return;
	}
	if(n == 2) {
		mdet = mtrx[0][0];
		mdet.calculateMultiply(mtrx[1][1], eo);
		mdet.add(mtrx[1][0], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[0][1], eo);
		mdet[mdet.size() - 1].calculateNegate(eo);
		mdet.calculateAddLast(eo);
		return;
	}
	if(n == 3) {
		mdet = mtrx[0][0];
		mdet.calculateMultiply(mtrx[1][1], eo);
		mdet.calculateMultiply(mtrx[2][2], eo);
		mdet.add(mtrx[0][0], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[1][2], eo);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[2][1], eo);
		mdet[mdet.size() - 1].calculateNegate(eo);
		mdet.calculateAddLast(eo);
		mdet.add(mtrx[0][1], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[1][0], eo);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[2][2], eo);
		mdet[mdet.size() - 1].calculateNegate(eo);
		mdet.calculateAddLast(eo);
		mdet.add(mtrx[0][2], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[1][0], eo);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[2][1], eo);
		mdet.calculateAddLast(eo);
		mdet.add(mtrx[0][1], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[1][2], eo);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[2][0], eo);
		mdet.calculateAddLast(eo);
		mdet.add(mtrx[0][2], true);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[1][1], eo);
		mdet[mdet.size() - 1].calculateMultiply(mtrx[2][0], eo);
		mdet[mdet.size() - 1].calculateNegate(eo);
		mdet.calculateAddLast(eo);		
		return;
	}
	
	std::vector<size_t> Pkey;
	Pkey.reserve(n);
	std::vector<size_t> Mkey;
	Mkey.reserve(n - 1);
	typedef std::map<std::vector<size_t>, class MathStructure> Rmap;
	typedef std::map<std::vector<size_t>, class MathStructure>::value_type Rmap_value;
	Rmap A;
	Rmap B;
	for(size_t r = 0; r < n; ++r) {
		Pkey.erase(Pkey.begin(), Pkey.end());
		Pkey.push_back(r);
		A.insert(Rmap_value(Pkey, mtrx[r][n - 1]));
	}
	for(long int c = n - 2; c >= 0; --c) {
		Pkey.erase(Pkey.begin(), Pkey.end()); 
		Mkey.erase(Mkey.begin(), Mkey.end());
		for(size_t i = 0; i < n - c; ++i) Pkey.push_back(i);
		size_t fc = 0;
		do {
			mdet.clear();
			for(size_t r = 0; r < n - c; ++r) {
				if (mtrx[Pkey[r]][c].isZero()) continue;
				Mkey.erase(Mkey.begin(), Mkey.end());
				for(size_t i = 0; i < n - c; ++i) {
					if(i != r) Mkey.push_back(Pkey[i]);
				}
				mdet.add(mtrx[Pkey[r]][c], true);
				mdet[mdet.size() - 1].calculateMultiply(A[Mkey], eo);
				if(r % 2) mdet[mdet.size() - 1].calculateNegate(eo);
				mdet.calculateAddLast(eo);
			}
			if(!mdet.isZero()) B.insert(Rmap_value(Pkey, mdet));
			for(fc = n - c; fc > 0; --fc) {
				++Pkey[fc-1];
				if(Pkey[fc - 1] < fc + c) break;
			}
			if(fc < n - c && fc > 0) {
				for(size_t j = fc; j < n - c; ++j) Pkey[j] = Pkey[j - 1] + 1;
			}
		} while(fc);
		A = B;
		B.clear();
	}	
	return;
}

//from GiNaC
int MathStructure::gaussianElimination(const EvaluationOptions &eo, bool det) {

	if(!isMatrix()) return 0;
	bool isnumeric = isNumericMatrix();

	size_t m = rows();
	size_t n = columns();
	int sign = 1;
	
	size_t r0 = 0;
	for(size_t c0 = 0; c0 < n && r0 < m - 1; ++c0) {
		int indx = pivot(r0, c0, true);
		if(indx == -1) {
			sign = 0;
			if(det) return 0;
		}
		if(indx >= 0) {
			if(indx > 0) sign = -sign;
			for(size_t r2 = r0 + 1; r2 < m; ++r2) {
				if(!CHILD(r2)[c0].isZero()) {
					if(isnumeric) {
						Number piv(CHILD(r2)[c0].number());
						piv /= CHILD(r0)[c0].number();
						for(size_t c = c0 + 1; c < n; ++c) {
							CHILD(r2)[c].number() -= piv * CHILD(r0)[c].number();
						}
					} else {
						MathStructure piv(CHILD(r2)[c0]);
						piv.calculateDivide(CHILD(r0)[c0], eo);
						for(size_t c = c0 + 1; c < n; ++c) {
							CHILD(r2)[c].add(piv, true);
							CHILD(r2)[c][CHILD(r2)[c].size() - 1].calculateMultiply(CHILD(r0)[c], eo);
							CHILD(r2)[c][CHILD(r2)[c].size() - 1].calculateNegate(eo);
							CHILD(r2)[c].calculateAddLast(eo);
						}
					}
				}
				for(size_t c = r0; c <= c0; ++c) CHILD(r2)[c].clear();
			}
			if(det) {
				for(size_t c = r0 + 1; c < n; ++c) CHILD(r0)[c].clear();
			}
			++r0;
		}
	}
	for(size_t r = r0 + 1; r < m; ++r) {
		for(size_t c = 0; c < n; ++c) CHILD(r)[c].clear();
	}

	return sign;
}

//from GiNaC
template <class It>
int permutation_sign(It first, It last)
{
	if (first == last)
		return 0;
	--last;
	if (first == last)
		return 0;
	It flag = first;
	int sign = 1;

	do {
		It i = last, other = last;
		--other;
		bool swapped = false;
		while (i != first) {
			if (*i < *other) {
				std::iter_swap(other, i);
				flag = other;
				swapped = true;
				sign = -sign;
			} else if (!(*other < *i))
				return 0;
			--i; --other;
		}
		if (!swapped)
			return sign;
		++flag;
		if (flag == last)
			return sign;
		first = flag;
		i = first; other = first;
		++other;
		swapped = false;
		while (i != last) {
			if (*other < *i) {
				std::iter_swap(i, other);
				flag = other;
				swapped = true;
				sign = -sign;
			} else if (!(*i < *other))
				return 0;
			++i; ++other;
		}
		if (!swapped)
			return sign;
		last = flag;
		--last;
	} while (first != last);

	return sign;
}

//from GiNaC
MathStructure &MathStructure::determinant(MathStructure &mstruct, const EvaluationOptions &eo) const {

	if(!matrixIsSquare()) {
		CALCULATOR->error(true, _("The determinant can only be calculated for square matrices."), NULL);
		mstruct = m_undefined;
		return mstruct;
	}

	if(SIZE == 1) {
		mstruct = CHILD(0)[0];
	} else if(isNumericMatrix()) {
	
		mstruct.set(1, 1, 0);
		MathStructure mtmp(*this);
		int sign = mtmp.gaussianElimination(eo, true);
		for(size_t d = 0; d < SIZE; ++d) {
			mstruct.number() *= mtmp[d][d].number();
		}
		mstruct.number() *= sign;
		
	} else {

		typedef std::pair<size_t, size_t> sizet_pair;
		std::vector<sizet_pair> c_zeros;
		for(size_t c = 0; c < CHILD(0).size(); ++c) {
			size_t acc = 0;
			for(size_t r = 0; r < SIZE; ++r) {
				if(CHILD(r)[c].isZero()) ++acc;
			}
			c_zeros.push_back(sizet_pair(acc, c));
		}

		std::sort(c_zeros.begin(), c_zeros.end());
		std::vector<size_t> pre_sort;
		for(std::vector<sizet_pair>::const_iterator i = c_zeros.begin(); i != c_zeros.end(); ++i) {
			pre_sort.push_back(i->second);
		}
		std::vector<size_t> pre_sort_test(pre_sort);

		int sign = permutation_sign(pre_sort_test.begin(), pre_sort_test.end());

		MathStructure result;
		result.clearMatrix();
		result.resizeMatrix(SIZE, CHILD(0).size(), m_zero);

		size_t c = 0;
		for(std::vector<size_t>::const_iterator i = pre_sort.begin(); i != pre_sort.end(); ++i,++c) {
			for(size_t r = 0; r < SIZE; ++r) result[r][c] = CHILD(r)[(*i)];
		}
		mstruct.clear();

		determinant_minor(result, mstruct, eo);

		if(sign != 1) {
			mstruct.calculateMultiply(sign, eo);
		}
		
	}
	
	mstruct.mergePrecision(*this);
	return mstruct;
	
}

MathStructure &MathStructure::permanent(MathStructure &mstruct, const EvaluationOptions &eo) const {
	if(!matrixIsSquare()) {
		CALCULATOR->error(true, _("The permanent can only be calculated for square matrices."), NULL);
		mstruct = m_undefined;
		return mstruct;
	}
	if(b_approx) mstruct.setApproximate();
	mstruct.setPrecision(i_precision);
	if(SIZE == 1) {
		if(CHILD(0).size() >= 1) {
			mstruct = CHILD(0)[0];
		}
	} else if(SIZE == 2) {
		mstruct = CHILD(0)[0];
		if(IS_REAL(mstruct) && IS_REAL(CHILD(1)[1])) {
			mstruct.number() *= CHILD(1)[1].number();
		} else {
			mstruct.calculateMultiply(CHILD(1)[1], eo);
		}
		if(IS_REAL(mstruct) && IS_REAL(CHILD(1)[0]) && IS_REAL(CHILD(0)[1])) {
			mstruct.number() += CHILD(1)[0].number() * CHILD(0)[1].number();
		} else {
			MathStructure mtmp = CHILD(1)[0];
			mtmp.calculateMultiply(CHILD(0)[1], eo);
			mstruct.calculateAdd(mtmp, eo);
		}
	} else {
		MathStructure mtrx;
		mtrx.clearMatrix();
		mtrx.resizeMatrix(SIZE - 1, CHILD(0).size() - 1, m_undefined);
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			for(size_t index_r2 = 1; index_r2 < SIZE; index_r2++) {
				for(size_t index_c2 = 0; index_c2 < CHILD(index_r2).size(); index_c2++) {
					if(index_c2 > index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2);
					} else if(index_c2 < index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2 + 1);
					}
				}
			}
			MathStructure mdet;
			mtrx.permanent(mdet, eo);
			if(IS_REAL(mdet) && IS_REAL(CHILD(0)[index_c])) {
				mdet.number() *= CHILD(0)[index_c].number();
			} else {
				mdet.calculateMultiply(CHILD(0)[index_c], eo);
			}
			if(IS_REAL(mdet) && IS_REAL(mstruct)) {
				mstruct.number() += mdet.number();
			} else {
				mstruct.calculateAdd(mdet, eo);
			}
		}
	}
	return mstruct;
}
void MathStructure::setToIdentityMatrix(size_t n) {
	clearMatrix();
	resizeMatrix(n, n, m_zero);
	for(size_t i = 0; i < n; i++) {
		CHILD(i)[i] = m_one;
	}
}
MathStructure &MathStructure::getIdentityMatrix(MathStructure &mstruct) const {
	mstruct.setToIdentityMatrix(columns());
	return mstruct;
}

//modified algorithm from eigenmath
bool MathStructure::invertMatrix(const EvaluationOptions &eo) {

	if(!matrixIsSquare()) return false;
	
	if(isNumericMatrix()) {
	
		int d, i, j, n = SIZE;

		MathStructure idstruct;
		Number mtmp;
		idstruct.setToIdentityMatrix(n);
		MathStructure mtrx(*this);

		for(d = 0; d < n; d++) {

			if(mtrx[d][d].isZero()) {

				for (i = d + 1; i < n; i++) {
					if(!mtrx[i][d].isZero()) break;
				}

				if(i == n) {
					CALCULATOR->error(true, _("Inverse of singular matrix."), NULL);
					return false;
				}

				mtrx[i].ref();
				mtrx[d].ref();
				MathStructure *mptr = &mtrx[d];
				mtrx.setChild_nocopy(&mtrx[i], d + 1);
				mtrx.setChild_nocopy(mptr, i + 1);
			
				idstruct[i].ref();
				idstruct[d].ref();
				mptr = &idstruct[d];
				idstruct.setChild_nocopy(&idstruct[i], d + 1);
				idstruct.setChild_nocopy(mptr, i + 1);
				
			}

			mtmp = mtrx[d][d].number();
			mtmp.recip();

			for(j = 0; j < n; j++) {

				if(j > d) {
					mtrx[d][j].number() *= mtmp;
				}

				idstruct[d][j].number() *= mtmp;
			
			}

			for(i = 0; i < n; i++) {

				if(i == d) continue;

				mtmp = mtrx[i][d].number();
				mtmp.negate();

				for(j = 0; j < n; j++) {
				
					if(j > d) {		
						mtrx[i][j].number() += mtrx[d][j].number() * mtmp;
					}
				
					idstruct[i][j].number() += idstruct[d][j].number() * mtmp;

				}
			}
		}
		set_nocopy(idstruct);
		MERGE_APPROX_AND_PREC(idstruct)
	} else {
		MathStructure *mstruct = new MathStructure();
		determinant(*mstruct, eo);
		mstruct->calculateInverse(eo);
		adjointMatrix(eo);
		multiply_nocopy(mstruct, true);
		calculateMultiplyLast(eo);
	}
	return true;
}

bool MathStructure::adjointMatrix(const EvaluationOptions &eo) {
	if(!matrixIsSquare()) return false;
	MathStructure msave(*this);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			msave.cofactor(index_r + 1, index_c + 1, CHILD(index_r)[index_c], eo);
		}
	}
	transposeMatrix();
	return true;
}
bool MathStructure::transposeMatrix() {
	MathStructure msave(*this);
	resizeMatrix(CHILD(0).size(), SIZE, m_undefined);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			CHILD(index_r)[index_c] = msave[index_c][index_r];
		}
	}
	return true;
}	
MathStructure &MathStructure::cofactor(size_t r, size_t c, MathStructure &mstruct, const EvaluationOptions &eo) const {
	if(r < 1) r = 1;
	if(c < 1) c = 1;
	if(r > SIZE || c > CHILD(0).size()) {
		mstruct = m_undefined;
		return mstruct;
	}
	r--; c--;
	mstruct.clearMatrix(); mstruct.resizeMatrix(SIZE - 1, CHILD(0).size() - 1, m_undefined);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		if(index_r != r) {
			for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
				if(index_c > c) {
					if(index_r > r) {
						mstruct[index_r - 1][index_c - 1] = CHILD(index_r)[index_c];
					} else {
						mstruct[index_r][index_c - 1] = CHILD(index_r)[index_c];
					}
				} else if(index_c < c) {
					if(index_r > r) {
						mstruct[index_r - 1][index_c] = CHILD(index_r)[index_c];
					} else {
						mstruct[index_r][index_c] = CHILD(index_r)[index_c];
					}
				}
			}
		}
	}	
	MathStructure mstruct2;
	mstruct = mstruct.determinant(mstruct2, eo);
	if((r + c) % 2 == 1) {
		mstruct.calculateNegate(eo);
	}
	return mstruct;
}

void gatherInformation(const MathStructure &mstruct, vector<Unit*> &base_units, vector<AliasUnit*> &alias_units) {
	switch(mstruct.type()) {
		case STRUCT_UNIT: {
			switch(mstruct.unit()->subtype()) {
				case SUBTYPE_BASE_UNIT: {
					for(size_t i = 0; i < base_units.size(); i++) {
						if(base_units[i] == mstruct.unit()) {
							return;
						}
					}
					base_units.push_back(mstruct.unit());
					break;
				}
				case SUBTYPE_ALIAS_UNIT: {
					for(size_t i = 0; i < alias_units.size(); i++) {
						if(alias_units[i] == mstruct.unit()) {
							return;
						}
					}
					alias_units.push_back((AliasUnit*) (mstruct.unit()));
					break;
				}
				case SUBTYPE_COMPOSITE_UNIT: {
					gatherInformation(((CompositeUnit*) (mstruct.unit()))->generateMathStructure(), base_units, alias_units);
					break;
				}				
			}
			break;
		}
		case STRUCT_FUNCTION: {
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(!mstruct.function()->getArgumentDefinition(i + 1) || mstruct.function()->getArgumentDefinition(i + 1)->type() != ARGUMENT_TYPE_ANGLE) {
					gatherInformation(mstruct[i], base_units, alias_units);
				}
			}
			break;
		}
		default: {
			for(size_t i = 0; i < mstruct.size(); i++) {
				gatherInformation(mstruct[i], base_units, alias_units);
			}
			break;
		}
	}

}

int MathStructure::isUnitCompatible(const MathStructure &mstruct) const {
	if(!isMultiplication() && mstruct.isMultiplication()) return mstruct.isUnitCompatible(*this);
	int b1 = mstruct.containsRepresentativeOfType(STRUCT_UNIT, true, true);
	int b2 = containsRepresentativeOfType(STRUCT_UNIT, true, true);
	if(b1 < 0 || b2 < 0) return -1;
	if(b1 != b2) return false;
	if(!b1) return true;
	if(isMultiplication()) {
		size_t unit_count1 = 0, unit_count2 = 0;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isUnit_exp()) unit_count1++;
			else if(CHILD(i).containsRepresentativeOfType(STRUCT_UNIT, true, true) != 0) return -1;
		}
		if(mstruct.isMultiplication()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(mstruct[i].isUnit_exp()) unit_count2++;
				else if(mstruct[i].containsRepresentativeOfType(STRUCT_UNIT, true, true) != 0) return -1;
			}
		} else if(mstruct.isUnit_exp()) {
			if(unit_count1 > 1) return false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isUnit_exp()) return CHILD(1) == mstruct;
			}
		} else {
			return -1;
		}
		if(unit_count1 != unit_count2) return false;
		size_t i2 = 0;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isUnit_exp()) {
				for(; i2 < mstruct.size(); i2++) {
					if(mstruct[i2].isUnit_exp()) {
						if(CHILD(i) != mstruct[i2]) return false;
						i2++;
						break;
					}
				}
			}
		}
	} else if(isUnit_exp()) {
		if(mstruct.isUnit_exp()) return equals(mstruct);
	}
	return -1;
}

bool MathStructure::syncUnits(bool sync_complex_relations, bool *found_complex_relations, bool calculate_new_functions, const EvaluationOptions &feo) {
	if(SIZE == 0) return false;
	vector<Unit*> base_units;
	vector<AliasUnit*> alias_units;
	vector<CompositeUnit*> composite_units;	
	gatherInformation(*this, base_units, alias_units);
	if(alias_units.empty() || base_units.size() + alias_units.size() == 1) return false;
	CompositeUnit *cu;
	bool b = false, do_erase = false;
	for(size_t i = 0; i < alias_units.size(); ) {
		do_erase = false;
		if(alias_units[i]->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			b = false;
			cu = (CompositeUnit*) alias_units[i]->baseUnit();
			for(size_t i2 = 0; i2 < base_units.size(); i2++) {
				if(cu->containsRelativeTo(base_units[i2])) {
					for(size_t i3 = 0; i3 < composite_units.size(); i3++) {
						if(composite_units[i3] == cu) {
							b = true;
							break;
						}
					}
					if(!b) composite_units.push_back(cu);
					do_erase = true;
					break;
				}
			}
			for(size_t i2 = 0; !do_erase && i2 < alias_units.size(); i2++) {
				if(i != i2 && cu->containsRelativeTo(alias_units[i2])) {
					for(size_t i3 = 0; i3 < composite_units.size(); i3++) {
						if(composite_units[i3] == cu) {
							b = true;
							break;
						}
					}
					if(!b) composite_units.push_back(cu);
					do_erase = true;
					break;
				}
			}					
		}
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			for(int i2 = 1; i2 <= (int) cu->countUnits(); i2++) {
				b = false;
				Unit *cub = cu->get(i2);
				switch(cub->subtype()) {
					case SUBTYPE_BASE_UNIT: {
						for(size_t i3 = 0; i3 < base_units.size(); i3++) {
							if(base_units[i3] == cub) {
								b = true;
								break;
							}
						}
						if(!b) base_units.push_back(cub);
						break;
					}
					case SUBTYPE_ALIAS_UNIT: {
						for(size_t i3 = 0; i3 < alias_units.size(); i3++) {
							if(alias_units[i3] == cub) {
								b = true;
								break;
							}
						}
						if(!b) alias_units.push_back((AliasUnit*) cub);
						break;
					}
					case SUBTYPE_COMPOSITE_UNIT: {
						gatherInformation(((CompositeUnit*) cub)->generateMathStructure(), base_units, alias_units);
						break;
					}
				}
			}
			i = 0;
		} else {
			i++;
		}
	}
	
	for(size_t i = 0; i < alias_units.size();) {
		do_erase = false;
		for(size_t i2 = 0; i2 < alias_units.size(); i2++) {
			if(i != i2 && alias_units[i]->baseUnit() == alias_units[i2]->baseUnit()) { 
				if(alias_units[i2]->isParentOf(alias_units[i])) {
					do_erase = true;
					break;
				} else if(!alias_units[i]->isParentOf(alias_units[i2])) {
					b = false;
					for(size_t i3 = 0; i3 < base_units.size(); i3++) {
						if(base_units[i3] == alias_units[i2]->firstBaseUnit()) {
							b = true;
							break;
						}
					}
					if(!b) base_units.push_back((Unit*) alias_units[i]->baseUnit());
					do_erase = true;
					break;
				}
			}
		} 
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			i = 0;
		} else {
			i++;
		}
	}	
	for(size_t i = 0; i < alias_units.size();) {
		do_erase = false;
		if(alias_units[i]->baseUnit()->subtype() == SUBTYPE_BASE_UNIT) {
			for(size_t i2 = 0; i2 < base_units.size(); i2++) {
				if(alias_units[i]->baseUnit() == base_units[i2]) {
					do_erase = true;
					break;
				}
			}
		} 
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			i = 0;
		} else {
			i++;
		}
	}
	b = false;
	bool fcr = false;
	for(size_t i = 0; i < composite_units.size(); i++) {
		if(convert(composite_units[i], sync_complex_relations, &fcr, calculate_new_functions, feo)) b = true;
	}	
	if(dissolveAllCompositeUnits()) b = true;
	for(size_t i = 0; i < base_units.size(); i++) {
		if(convert(base_units[i], sync_complex_relations, &fcr, calculate_new_functions, feo)) b = true;
	}
	for(size_t i = 0; i < alias_units.size(); i++) {
		if(convert(alias_units[i], sync_complex_relations, &fcr, calculate_new_functions, feo)) b = true;
	}
	if(sync_complex_relations && fcr) CALCULATOR->error(false, _("Calculations involving conversion of units without proportional linear relationship (e.g. with multiple temperature units), might give unexpected results and is not recommended."), NULL);
	if(fcr && found_complex_relations) *found_complex_relations = fcr;
	return b;
}
bool MathStructure::testDissolveCompositeUnit(Unit *u) {
	if(m_type == STRUCT_UNIT) {
		if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) o_unit)->containsRelativeTo(u)) {
				set(((CompositeUnit*) o_unit)->generateMathStructure());
				return true;
			}
		} else if(o_unit->subtype() == SUBTYPE_ALIAS_UNIT && o_unit->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) (o_unit->baseUnit()))->containsRelativeTo(u)) {
				convert(o_unit->baseUnit());
				convert(u);
				return true;
			}		
		}
	}
	return false; 
}
bool MathStructure::testCompositeUnit(Unit *u) {
	if(m_type == STRUCT_UNIT) {
		if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) o_unit)->containsRelativeTo(u)) {
				return true;
			}
		} else if(o_unit->subtype() == SUBTYPE_ALIAS_UNIT && o_unit->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) (o_unit->baseUnit()))->containsRelativeTo(u)) {
				return true;
			}		
		}
	}
	return false; 
}
bool MathStructure::dissolveAllCompositeUnits() {
	switch(m_type) {
		case STRUCT_UNIT: {
			if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
				set(((CompositeUnit*) o_unit)->generateMathStructure());
				return true;
			}
			break;
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).dissolveAllCompositeUnits()) {
					CHILD_UPDATED(i);
					b = true;
				}
			}
			return b;
		}		
	}		
	return false;
}
bool MathStructure::setPrefixForUnit(Unit *u, Prefix *new_prefix) {
	if(m_type == STRUCT_UNIT && o_unit == u) {
		if(o_prefix != new_prefix) {
			Number new_value(1, 1);
			if(o_prefix) new_value = o_prefix->value();
			if(new_prefix) new_value.divide(new_prefix->value());
			o_prefix = new_prefix;
			multiply(new_value);
			return true;
		}
		return false;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).setPrefixForUnit(u, new_prefix)) {
			CHILD_UPDATED(i);
			b = true;
		}
	}
	return b;
}

bool searchSubMultiplicationsForComplexRelations(Unit *u, const MathStructure &mstruct) {
	int b_c = -1;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(mstruct[i].isUnit_exp()) {
			if((mstruct[i].isUnit() && u->hasComplexRelationTo(mstruct[i].unit())) || (mstruct[i].isPower() && u->hasComplexRelationTo(mstruct[i][0].unit()))) {
				return true;
			}
		} else if(b_c == -1 && mstruct[i].isMultiplication()) {
			b_c = -3;
		}
	}
	if(b_c == -3) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isMultiplication() && searchSubMultiplicationsForComplexRelations(u, mstruct[i])) return true;
		}
	}
	return false;
}
bool MathStructure::convertToBaseUnits(bool convert_complex_relations, bool *found_complex_relations, bool calculate_new_functions, const EvaluationOptions &feo) {
	if(m_type == STRUCT_UNIT) {
		if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			set(((CompositeUnit*) o_unit)->generateMathStructure());
			convertToBaseUnits(convert_complex_relations, found_complex_relations, calculate_new_functions, feo);
			return true;
		} else if(o_unit->subtype() == SUBTYPE_ALIAS_UNIT) {
			AliasUnit *au = (AliasUnit*) o_unit;
			if(au->hasComplexRelationTo(au->baseUnit())) {
				if(found_complex_relations) *found_complex_relations = true;
				if(!convert_complex_relations) {
					if(!au->hasComplexExpression()) {
						MathStructure mstruct_old(*this);
						if(convert(au->firstBaseUnit(), false, NULL, calculate_new_functions, feo) && !equals(mstruct_old)) {
							convertToBaseUnits(false, NULL, calculate_new_functions, feo);
							return true;
						}
					}
					return false;
				}
			}
			if(convert(au->baseUnit(), convert_complex_relations, found_complex_relations, calculate_new_functions, feo)) {
				convertToBaseUnits(convert_complex_relations, found_complex_relations, calculate_new_functions, feo);
				return true;
			}
		}
		return false;
	} else if(m_type == STRUCT_MULTIPLICATION && (convert_complex_relations || found_complex_relations)) {
		AliasUnit *complex_au = NULL;
		if(convert_complex_relations && convertToBaseUnits(false, NULL, calculate_new_functions, feo)) {
			return true;
		}
		for(size_t i = 0; i < SIZE; i++) {
			if(SIZE > 100 && CALCULATOR->aborted()) return false;
			if(CHILD(i).isUnit_exp() && CHILD(i).unit_exp_unit()->subtype() == SUBTYPE_ALIAS_UNIT) {
				AliasUnit *au = (AliasUnit*) CHILD(i).unit_exp_unit();
				if(au && au->hasComplexRelationTo(au->baseUnit())) {
					if(found_complex_relations) {
						*found_complex_relations = true;
					}
					if(convert_complex_relations) {
						if(complex_au) {
							complex_au = NULL;
							convert_complex_relations = false;
							break;
						} else {
							complex_au = au;
						}
					} else {
						break;
					}
				}
			}
		}
		if(convert_complex_relations && complex_au) {
			MathStructure mstruct_old(*this);
			if(convert(complex_au->firstBaseUnit(), true, NULL, calculate_new_functions, feo)) {
				return true;
			}
		}
	} else if(m_type == STRUCT_FUNCTION) {
		bool b = false;
		for(size_t i = 0; i < SIZE; i++) {
			if(SIZE > 100 && CALCULATOR->aborted()) return b;
			if((!o_function->getArgumentDefinition(i + 1) || o_function->getArgumentDefinition(i + 1)->type() != ARGUMENT_TYPE_ANGLE) && CHILD(i).convertToBaseUnits(convert_complex_relations, found_complex_relations, calculate_new_functions, feo)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
		return b;
	}
	bool b = false; 
	for(size_t i = 0; i < SIZE; i++) {
		if(SIZE > 100 && CALCULATOR->aborted()) return b;
		if(CHILD(i).convertToBaseUnits(convert_complex_relations, found_complex_relations, calculate_new_functions, feo)) {
			CHILD_UPDATED(i);
			b = true;
		}
	}
	return b;
}
bool MathStructure::convert(Unit *u, bool convert_complex_relations, bool *found_complex_relations, bool calculate_new_functions, const EvaluationOptions &feo, Prefix *new_prefix) {
	bool b = false;
	if(m_type == STRUCT_UNIT && o_unit == u) {
		if((new_prefix || o_prefix) && o_prefix != new_prefix) {
			Number new_value(1, 1);
			if(o_prefix) new_value = o_prefix->value();
			if(new_prefix) new_value.divide(new_prefix->value());
			o_prefix = new_prefix;
			multiply(new_value);
			return true;
		}
		return false;
	}
	if(u->subtype() == SUBTYPE_COMPOSITE_UNIT && !(m_type == STRUCT_UNIT && o_unit->baseUnit() == u)) {
		return convert(((CompositeUnit*) u)->generateMathStructure(false, true), convert_complex_relations, found_complex_relations, calculate_new_functions, feo);
	}
	if(m_type == STRUCT_UNIT) {
		if(u->hasComplexRelationTo(o_unit)) {
			if(found_complex_relations) *found_complex_relations = true;			
			if(!convert_complex_relations) return false;
		}
		if(o_unit->baseUnit() != u->baseUnit() && testDissolveCompositeUnit(u)) {
			convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
			return true;
		}
		MathStructure *exp = new MathStructure(1, 1, 0);
		MathStructure *mstruct = new MathStructure(1, 1, 0);
		if(o_prefix) {
			mstruct->set(o_prefix->value());
		}
		if(u->convert(o_unit, *mstruct, *exp)) {
			o_unit = u;
			o_prefix = new_prefix;
			if(new_prefix) {
				divide(new_prefix->value());
			}
			if(!exp->isOne()) {
				if(calculate_new_functions) exp->calculateFunctions(feo, true, false);
				raise_nocopy(exp);
			} else {
				exp->unref();
			}			
			if(!mstruct->isOne()) {
				if(calculate_new_functions) mstruct->calculateFunctions(feo, true, false);
				multiply_nocopy(mstruct);
			} else {
				mstruct->unref();
			}			
			return true;
		} else {
			exp->unref();
			mstruct->unref();
			return false;
		}
	} else {
		if(convert_complex_relations || found_complex_relations) {
			if(m_type == STRUCT_MULTIPLICATION) {
				long int b_c = -1;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isUnit_exp()) {
						if((CHILD(i).isUnit() && u->hasComplexRelationTo(CHILD(i).unit())) || (CHILD(i).isPower() && u->hasComplexRelationTo(CHILD(i)[0].unit()))) {
							if(found_complex_relations) *found_complex_relations = true;
							
							b_c = i;
							break;
						}
					} else if(CHILD(i).isMultiplication()) {
						b_c = -3;
					}
				}
				if(b_c == -3) {
					for(size_t i = 0; i < SIZE; i++) {
						if(CHILD(i).isMultiplication()) {
							if(searchSubMultiplicationsForComplexRelations(u, CHILD(i))) {
								if(!convert_complex_relations) {
									*found_complex_relations = true;
									break;
								}
								flattenMultiplication(*this);
								return convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
							}
						}
					}
				}
				if(convert_complex_relations && b_c >= 0) {
					if(flattenMultiplication(*this)) return convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
					MathStructure mstruct(1, 1);
					MathStructure mstruct2(1, 1);
					if(SIZE == 2) {
						if(b_c == 0) mstruct = CHILD(1);
						else mstruct = CHILD(0);
						if(mstruct.isUnit_exp()) {
							mstruct2 = mstruct;
							mstruct.set(1, 1, 0);
						}
					} else if(SIZE > 2) {
						mstruct = *this;
						size_t nr_of_del = 0;
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isUnit_exp()) {
								mstruct.delChild(i + 1 - nr_of_del);
								nr_of_del++;
								if((long int) i != b_c) {
									if(mstruct2.isOne()) mstruct2 = CHILD(i);
									else mstruct2.multiply(CHILD(i), true);
								}
							}
						}
						if(mstruct.size() == 1) mstruct.setToChild(1);
						else if(mstruct.size() == 0) mstruct.set(1, 1, 0);
					}
					MathStructure exp(1, 1);
					Unit *u2;
					bool b_p = false;
					if(CHILD(b_c).isPower()) {
						if(CHILD(b_c)[0].testDissolveCompositeUnit(u)) {
							convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
							return true;
						}	
						exp = CHILD(b_c)[1];
						u2 = CHILD(b_c)[0].unit();
						if(CHILD(b_c)[0].prefix()) b_p = true;
					} else {
						if(CHILD(b_c).testDissolveCompositeUnit(u)) {
							convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
							return true;
						}
						u2 = CHILD(b_c).unit();
						if(CHILD(b_c).prefix()) b_p = true;
					}
					size_t efc = 0, mfc = 0;
					if(calculate_new_functions) {
						efc = exp.countFunctions();
						mfc = mstruct.countFunctions();
					}
					if(u->convert(u2, mstruct, exp)) {
						if(b_p) {
							unformat();
							return convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
						}
						set(u);
						if(!exp.isOne()) {
							if(calculate_new_functions && exp.countFunctions() > efc) exp.calculateFunctions(feo, true, false);
							raise(exp);
						}
						if(!mstruct2.isOne()) {
							multiply(mstruct2);
						}
						if(!mstruct.isOne()) {
							if(calculate_new_functions && mstruct.countFunctions() > mfc) mstruct.calculateFunctions(feo, true, false);
							multiply(mstruct);
						}
						return true;
					}
					return false;
				}
			} else if(m_type == STRUCT_POWER) {
				if(CHILD(0).isUnit() && u->hasComplexRelationTo(CHILD(0).unit())) {
					if(found_complex_relations) *found_complex_relations = true;
					if(convert_complex_relations) {
						if(CHILD(0).testDissolveCompositeUnit(u)) {
							convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix);
							return true;
						}	
						MathStructure exp(CHILD(1));
						MathStructure mstruct(1, 1);
						if(CHILD(0).prefix()) {
							mstruct.set(CHILD(0).prefix()->value());
							mstruct.raise(exp);
						}
						size_t efc = 0;
						if(calculate_new_functions) {
							efc = exp.countFunctions();
						}
						if(u->convert(CHILD(0).unit(), mstruct, exp)) {
							set(u);
							if(!exp.isOne()) {
								if(calculate_new_functions && exp.countFunctions() > efc) exp.calculateFunctions(feo, true, false);
								raise(exp);
							}
							if(!mstruct.isOne()) {
								if(calculate_new_functions) mstruct.calculateFunctions(feo, true, false);
								multiply(mstruct);
							}
							return true;
						}
						return false;
					}
				}
			}
			/*if(m_type == STRUCT_MULTIPLICATION || m_type == STRUCT_POWER) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).convert(u, false, found_complex_relations, calculate_new_functions, feo, new_prefix)) {
						CHILD_UPDATED(i);
						b = true;
					}
				}
				return b;
			}*/
		}
		if(m_type == STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				if(SIZE > 100 && CALCULATOR->aborted()) return b;
				if((!o_function->getArgumentDefinition(i + 1) || o_function->getArgumentDefinition(i + 1)->type() != ARGUMENT_TYPE_ANGLE) && CHILD(i).convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix)) {
					CHILD_UPDATED(i);
					b = true;
				}
			}
			return b;
		}
		for(size_t i = 0; i < SIZE; i++) {
			if(SIZE > 100 && CALCULATOR->aborted()) return b;
			if(CHILD(i).convert(u, convert_complex_relations, found_complex_relations, calculate_new_functions, feo, new_prefix)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
		return b;
	}
	return b;
}
bool MathStructure::convert(const MathStructure unit_mstruct, bool convert_complex_relations, bool *found_complex_relations, bool calculate_new_functions, const EvaluationOptions &feo) {
	bool b = false;
	if(unit_mstruct.type() == STRUCT_UNIT) {
		if(convert(unit_mstruct.unit(), convert_complex_relations, found_complex_relations, calculate_new_functions, feo, feo.keep_prefixes ? unit_mstruct.prefix() : NULL)) b = true;
	} else {
		for(size_t i = 0; i < unit_mstruct.size(); i++) {
			if(convert(unit_mstruct[i], convert_complex_relations, found_complex_relations, calculate_new_functions, feo)) b = true;
		}
	}	
	return b;
}

int MathStructure::contains(const MathStructure &mstruct, bool structural_only, bool check_variables, bool check_functions) const {
	if(equals(mstruct)) return 1;
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).contains(mstruct)) return 1;
		}
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).contains(mstruct, structural_only, check_variables, check_functions);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
			return ((KnownVariable*) o_variable)->get().contains(mstruct, structural_only, check_variables, check_functions);
		} else if(m_type == STRUCT_FUNCTION && check_functions) {
			if(function_value) {
				return function_value->contains(mstruct, structural_only, check_variables, check_functions);
			}
			return -1;
		} else if(isAborted()) {
			return -1;
		}
		return ret;
	}
	return 0;
}
int MathStructure::containsFunction(MathFunction *f, bool structural_only, bool check_variables, bool check_functions) const {
	if(m_type == STRUCT_FUNCTION && o_function == f) return 1;
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).containsFunction(f)) return 1;
		}
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).containsFunction(f, structural_only, check_variables, check_functions);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
			return ((KnownVariable*) o_variable)->get().containsFunction(f, structural_only, check_variables, check_functions);
		} else if(m_type == STRUCT_FUNCTION && check_functions) {
			if(function_value) {
				return function_value->containsFunction(f, structural_only, check_variables, check_functions);
			}
			return -1;
		} else if(isAborted()) {
			return -1;
		}
		return ret;
	}
	return 0;
}
int MathStructure::containsInterval(bool structural_only, bool check_variables, bool check_functions, bool ignore_high_precision_interval) const {
	if(m_type == STRUCT_NUMBER && o_number.isInterval(false)) {
		if(ignore_high_precision_interval) {
			if(o_number.precision(true) > PRECISION + 10) return 0;
		}
		return 1;
	}
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).containsInterval(structural_only, check_variables, check_functions, ignore_high_precision_interval)) return 1;
		}
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).containsInterval(structural_only, check_variables, check_functions, ignore_high_precision_interval);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
			return ((KnownVariable*) o_variable)->get().containsInterval(structural_only, check_variables, check_functions), ignore_high_precision_interval;
		} else if(m_type == STRUCT_FUNCTION && check_functions) {
			if(function_value) {
				return function_value->containsInterval(structural_only, check_variables, check_functions, ignore_high_precision_interval);
			}
			return -1;
		} else if(isAborted()) {
			return -1;
		}
		return ret;
	}
	return 0;
}

int MathStructure::containsRepresentativeOf(const MathStructure &mstruct, bool check_variables, bool check_functions) const {
	if(equals(mstruct)) return 1;
	int ret = 0;
	if(m_type != STRUCT_FUNCTION) {
		for(size_t i = 0; i < SIZE; i++) {
			int retval = CHILD(i).containsRepresentativeOf(mstruct, check_variables, check_functions);
			if(retval == 1) return 1;
			else if(retval < 0) ret = retval;
		}
	}
	if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
		return ((KnownVariable*) o_variable)->get().containsRepresentativeOf(mstruct, check_variables, check_functions);
	} else if(m_type == STRUCT_FUNCTION && check_functions) {
		if(function_value) {
			return function_value->containsRepresentativeOf(mstruct, check_variables, check_functions);
		}
		if(!mstruct.isNumber() && (o_function->isBuiltin() || representsNumber())) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).containsRepresentativeOf(mstruct, check_variables, check_functions);
				if(retval != 0) return -1;
			}
			return 0;
		}
		return -1;
	} else if(isAborted()) {
		return -1;
	}
	return ret;
}

int MathStructure::containsType(StructureType mtype, bool structural_only, bool check_variables, bool check_functions) const {
	if(m_type == mtype) return 1;
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).containsType(mtype, true, check_variables, check_functions)) return 1;
		}
		return 0;
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).containsType(mtype, false, check_variables, check_functions);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(check_variables && m_type == STRUCT_VARIABLE && o_variable->isKnown()) {
			return ((KnownVariable*) o_variable)->get().containsType(mtype, false, check_variables, check_functions);
		} else if(check_functions && m_type == STRUCT_FUNCTION) {
			if(function_value) {
				return function_value->containsType(mtype, false, check_variables, check_functions);
			}
			return -1;
		} else if(isAborted()) {
			return -1;
		}
		return ret;
	}
}
int MathStructure::containsRepresentativeOfType(StructureType mtype, bool check_variables, bool check_functions) const {
	if(m_type == mtype) return 1;
	int ret = 0;
	if(m_type != STRUCT_FUNCTION) {
		for(size_t i = 0; i < SIZE; i++) {
			int retval = CHILD(i).containsRepresentativeOfType(mtype, check_variables, check_functions);
			if(retval == 1) return 1;
			else if(retval < 0) ret = retval;
		}
	}
	if(check_variables && m_type == STRUCT_VARIABLE && o_variable->isKnown()) {
		return ((KnownVariable*) o_variable)->get().containsRepresentativeOfType(mtype, check_variables, check_functions);
	} else if(check_functions && m_type == STRUCT_FUNCTION) {
		if(function_value) {
			return function_value->containsRepresentativeOfType(mtype, check_variables, check_functions);
		}
	}
	if(m_type == STRUCT_SYMBOLIC || m_type == STRUCT_VARIABLE || m_type == STRUCT_FUNCTION || m_type == STRUCT_ABORTED) {
		if(representsNumber(false)) {
			if(mtype == STRUCT_UNIT) return -1;
			return mtype == STRUCT_NUMBER;
		} else {
			return -1;
		}
	}
	return ret;
}
bool MathStructure::containsUnknowns() const {
	if(isUnknown()) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsUnknowns()) return true;
	}
	return false;
}
bool MathStructure::containsDivision() const {
	if(m_type == STRUCT_DIVISION || m_type == STRUCT_INVERSE || (m_type == STRUCT_POWER && CHILD(1).hasNegativeSign())) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsDivision()) return true;
	}
	return false;
}
size_t MathStructure::countFunctions(bool count_subfunctions) const {
	size_t c = 0;
	if(isFunction()) {
		if(!count_subfunctions) return 1;
		c = 1;
	}
	for(size_t i = 0; i < SIZE; i++) {
		c += CHILD(i).countFunctions();
	}
	return c;
}
void MathStructure::findAllUnknowns(MathStructure &unknowns_vector) {
	if(!unknowns_vector.isVector()) unknowns_vector.clearVector();
	switch(m_type) {
		case STRUCT_VARIABLE: {
			if(o_variable->isKnown()) {
				break;
			}
		}
		case STRUCT_SYMBOLIC: {
			bool b = false;
			for(size_t i = 0; i < unknowns_vector.size(); i++) {
				if(equals(unknowns_vector[i])) {
					b = true;
					break;
				}
			}
			if(!b) unknowns_vector.addChild(*this);
			break;
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).findAllUnknowns(unknowns_vector);
			}
		}
	}
}
bool MathStructure::replace(const MathStructure &mfrom, const MathStructure &mto, bool once_only) {
	if(b_protected) b_protected = false;
	if(equals(mfrom, true)) {
		set(mto);
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).replace(mfrom, mto, once_only)) {
			b = true;
			CHILD_UPDATED(i);
			if(once_only) return true;
		}
	}
	return b;
}
bool MathStructure::calculateReplace(const MathStructure &mfrom, const MathStructure &mto, const EvaluationOptions &eo) {
	if(equals(mfrom, true)) {
		set(mto);
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).calculateReplace(mfrom, mto, eo)) {
			b = true;
			CHILD_UPDATED(i);
		}
	}
	if(b) {
		calculatesub(eo, eo, false);
	}
	return b;
}

bool MathStructure::replace(const MathStructure &mfrom1, const MathStructure &mto1, const MathStructure &mfrom2, const MathStructure &mto2) {
	if(equals(mfrom1, true)) {
		set(mto1);
		return true;
	}
	if(equals(mfrom2, true)) {
		set(mto2);
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).replace(mfrom1, mto1, mfrom2, mto2)) {
			b = true;
			CHILD_UPDATED(i);
		}
	}
	return b;
}
bool MathStructure::removeType(StructureType mtype) {
	if(m_type == mtype || (m_type == STRUCT_POWER && CHILD(0).type() == mtype)) {
		set(1);
		return true;
	}
	bool b = false;
	if(m_type == STRUCT_MULTIPLICATION) {
		for(long int i = 0; i < (long int) SIZE; i++) {
			if(CHILD(i).removeType(mtype)) {
				if(CHILD(i).isOne()) {
					ERASE(i);
					i--;
				} else {
					CHILD_UPDATED(i);
				}
				b = true;
			}
		}
		if(SIZE == 0) {
			set(1);
		} else if(SIZE == 1) {
			setToChild(1, true);
		}
	} else {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).removeType(mtype)) {
				b = true;
				CHILD_UPDATED(i);
			}
		}
	}
	return b;
}

MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &min, const MathStructure &max, int steps, MathStructure *x_vector, const EvaluationOptions &eo) const {
	if(steps < 1) {
		steps = 1;
	}
	MathStructure x_value(min);
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	if(steps > 1000000) {
		CALCULATOR->error(true, _("Too many data points"), NULL);
		return y_vector;
	}
	MathStructure step(max);
	step.calculateSubtract(min, eo);
	step.calculateDivide(steps - 1, eo);
	if(!step.isNumber() || step.number().isNegative()) {
		CALCULATOR->error(true, _("The selected min and max do not result in a positive, finite number of data points"), NULL);
		return y_vector;
	}
	y_vector.resizeVector(steps, m_zero);
	if(x_vector) x_vector->resizeVector(steps, m_zero);
	for(int i = 0; i < steps; i++) {
		if(x_vector) {
			(*x_vector)[i] = x_value;
		}
		y_value = *this;
		y_value.replace(x_mstruct, x_value);
		y_value.eval(eo);
		y_vector[i] = y_value;
		if(x_value.isNumber()) x_value.number().add(step.number());
		else x_value.calculateAdd(step, eo);
		if(CALCULATOR->aborted()) {
			y_vector.resizeVector(i, m_zero);
			if(x_vector) x_vector->resizeVector(i, m_zero);
			return y_vector;
		}
	}
	return y_vector;
}
MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &min, const MathStructure &max, const MathStructure &step, MathStructure *x_vector, const EvaluationOptions &eo) const {
	MathStructure x_value(min);
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	if(min != max) {
		MathStructure mtest(max);
		mtest.calculateSubtract(min, eo);
		if(!step.isZero()) mtest.calculateDivide(step, eo);
		if(step.isZero() || !mtest.isNumber() || mtest.number().isNegative()) {
			CALCULATOR->error(true, _("The selected min, max and step size do not result in a positive, finite number of data points"), NULL);
			return y_vector;
		} else if(mtest.number().isGreaterThan(1000000)) {
			CALCULATOR->error(true, _("Too many data points"), NULL);
			return y_vector;
		}
		mtest.number().round();
		unsigned int steps = mtest.number().uintValue();
		y_vector.resizeVector(steps, m_zero);
		if(x_vector) x_vector->resizeVector(steps, m_zero);
	}
	ComparisonResult cr = max.compare(x_value);
	size_t i = 0;
	while(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
		if(x_vector) {
			if(i >= x_vector->size()) x_vector->addChild(x_value);
			else (*x_vector)[i] = x_value;
		}
		y_value = *this;
		y_value.replace(x_mstruct, x_value);
		y_value.eval(eo);
		if(i >= y_vector.size()) y_vector.addChild(y_value);
		else y_vector[i] = y_value;
		if(x_value.isNumber()) x_value.number().add(step.number());
		else x_value.calculateAdd(step, eo);
		cr = max.compare(x_value);
		if(CALCULATOR->aborted()) {
			y_vector.resizeVector(i, m_zero);
			if(x_vector) x_vector->resizeVector(i, m_zero);
			return y_vector;
		}
		i++;
	}
	y_vector.resizeVector(i, m_zero);
	if(x_vector) x_vector->resizeVector(i, m_zero);
	return y_vector;
}
MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &x_vector, const EvaluationOptions &eo) const {
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	for(size_t i = 1; i <= x_vector.countChildren(); i++) {
		if(CALCULATOR->aborted()) {
			y_vector.clearVector();
			return y_vector;
		}
		y_value = *this;
		y_value.replace(x_mstruct, x_vector.getChild(i));
		y_value.eval(eo);
		y_vector.addChild(y_value);
	}
	return y_vector;
}

void remove_zero_mul(MathStructure &m);
void remove_zero_mul(MathStructure &m) {
	if(m.isMultiplication()) {
		for(size_t i = 0; i < m.size(); i++) {
			if(m[i].isZero()) {
				m.clear(true);
				return;
			}
		}
	}
	for(size_t i = 0; i < m.size(); i++) {
		remove_zero_mul(m[i]);
	}
}
bool MathStructure::differentiate(const MathStructure &x_var, const EvaluationOptions &eo) {
	if(equals(x_var)) {
		set(m_one);
		return true;
	}
	switch(m_type) {
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).differentiate(x_var, eo)) {
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_LOGICAL_AND: {
			bool b = true;
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).isComparison()) {b = false; break;}
			}
			if(b) {
				MathStructure mtest(*this);
				mtest.setType(STRUCT_MULTIPLICATION);
				if(mtest.differentiate(x_var, eo) && mtest.containsFunction(CALCULATOR->f_diff, true) <= 0) {
					set(mtest);
					break;
				}
			}
		}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_XOR: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {
			if(containsRepresentativeOf(x_var, true, true) != 0) {
				MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
				set(mstruct);
				return false;
			}
			clear(true);
			break;
		}
		case STRUCT_COMPARISON: {
			if(containsRepresentativeOf(x_var, true, true) != 0) {
				if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
					if(!CHILD(1).isZero()) CHILD(0) -= CHILD(1);
					SET_CHILD_MAP(0)
					if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_LESS) negate();
					MathStructure mstruct(*this);
					MathStructure mstruct2(*this);
					transform(CALCULATOR->f_heaviside);
					transform(CALCULATOR->f_dirac);
					mstruct2.transform(CALCULATOR->f_dirac);
					multiply(mstruct2);
					if(ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_EQUALS_LESS) multiply_nocopy(new MathStructure(2, 1, 0));
					else multiply_nocopy(new MathStructure(-2, 1, 0));
					mstruct.differentiate(x_var, eo);
					multiply(mstruct);
					return true;
				}
				MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
				set(mstruct);
				return false;
			}
		}
		case STRUCT_UNIT: {}
		case STRUCT_NUMBER: {
			clear(true);
			break;
		}
		case STRUCT_POWER: {
			if(SIZE < 1) {
				clear(true);
				break;
			} else if(SIZE < 2) {
				setToChild(1, true);
				return differentiate(x_var, eo);
			}
			bool x_in_base = CHILD(0).containsRepresentativeOf(x_var, true, true) != 0;
			bool x_in_exp = CHILD(1).containsRepresentativeOf(x_var, true, true) != 0;
			if(x_in_base && !x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure base_mstruct(CHILD(0));
				if(!CHILD(1).isNumber() || !CHILD(1).number().add(-1)) CHILD(1) += m_minus_one;
				multiply(exp_mstruct);
				base_mstruct.differentiate(x_var, eo);
				multiply(base_mstruct);
			} else if(!x_in_base && x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure mstruct(CALCULATOR->f_ln, &CHILD(0), NULL);
				multiply(mstruct);
				exp_mstruct.differentiate(x_var, eo);
				multiply(exp_mstruct);
			} else if(x_in_base && x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure base_mstruct(CHILD(0));
				exp_mstruct.differentiate(x_var, eo);
				base_mstruct.differentiate(x_var, eo);
				base_mstruct /= CHILD(0);
				base_mstruct *= CHILD(1);
				MathStructure mstruct(CALCULATOR->f_ln, &CHILD(0), NULL);
				mstruct *= exp_mstruct;
				mstruct += base_mstruct;
				multiply(mstruct);
			} else {
				clear(true);
			}
			break;
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_sqrt && SIZE == 1) {
				if(CHILD(0).containsRepresentativeOf(x_var, true, true) != 0) {
					MathStructure base_mstruct(CHILD(0));
					raise(m_minus_one);
					multiply(nr_half);
					base_mstruct.differentiate(x_var, eo);
					multiply(base_mstruct);
				} else {
					clear(true);
				}
			} else if(o_function == CALCULATOR->f_root && THIS_VALID_ROOT) {
				if(CHILD(0).containsRepresentativeOf(x_var, true, true) != 0) {
					MathStructure base_mstruct(CHILD(0));
					MathStructure mexp(CHILD(1));
					mexp.negate();
					mexp += m_one;
					raise(mexp);
					divide(CHILD(0)[1]);
					base_mstruct.differentiate(x_var, eo);
					multiply(base_mstruct);
				} else {
					clear(true);
				}
			} else if(o_function == CALCULATOR->f_ln && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				setToChild(1, true);
				inverse();
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_erf && SIZE == 1) {
				MathStructure mdiff(CHILD(0));
				MathStructure mexp(CHILD(0));
				mexp ^= nr_two;
				mexp.negate();
				set(CALCULATOR->v_e, true);
				raise(mexp);
				multiply(nr_two);
				multiply(CALCULATOR->v_pi);
				LAST ^= Number(-1, 2);
				mdiff.differentiate(x_var, eo);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_li && SIZE == 1) {
				setFunction(CALCULATOR->f_ln);
				MathStructure mstruct(CHILD(0));
				inverse();
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_Li && SIZE == 2) {
				CHILD(0) += m_minus_one;
				MathStructure mstruct(CHILD(1));
				divide(mstruct);
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_Ei && SIZE == 1) {
				MathStructure mexp(CALCULATOR->v_e);
				mexp ^= CHILD(0);
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				mexp *= mdiff;
				setToChild(1, true);
				inverse();
				multiply(mexp);
			} else if(o_function == CALCULATOR->f_Si && SIZE == 1) {
				setFunction(CALCULATOR->f_sin);
				MathStructure marg(CHILD(0));
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				divide(marg);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_Ci && SIZE == 1) {
				setFunction(CALCULATOR->f_cos);
				MathStructure marg(CHILD(0));
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				divide(marg);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_Shi && SIZE == 1) {
				setFunction(CALCULATOR->f_sinh);
				MathStructure marg(CHILD(0));
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				divide(marg);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_Chi && SIZE == 1) {
				setFunction(CALCULATOR->f_cosh);
				MathStructure marg(CHILD(0));
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				divide(marg);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_abs && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				inverse();
				multiply(mstruct);
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_signum && SIZE == 2) {
				MathStructure mstruct(CHILD(0));
				ERASE(1)
				setFunction(CALCULATOR->f_dirac);
				multiply_nocopy(new MathStructure(2, 1, 0));
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_heaviside && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				setFunction(CALCULATOR->f_dirac);
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_lambert_w && SIZE == 1 && CHILD(0) == x_var) {
				MathStructure *mstruct = new MathStructure(*this);
				mstruct->add(m_one);
				mstruct->multiply(CHILD(0));
				divide_nocopy(mstruct);
			} else if(o_function == CALCULATOR->f_sin && SIZE == 1) {
				setFunction(CALCULATOR->f_cos);
				MathStructure mstruct(CHILD(0));
				mstruct.calculateDivide(CALCULATOR->getRadUnit(), eo);
				if(mstruct != x_var) {
					mstruct.differentiate(x_var, eo);
					multiply(mstruct);
				}
			} else if(o_function == CALCULATOR->f_cos && SIZE == 1) {
				setFunction(CALCULATOR->f_sin);
				MathStructure mstruct(CHILD(0));
				negate();
				mstruct.calculateDivide(CALCULATOR->getRadUnit(), eo);
				if(mstruct != x_var) {
					mstruct.differentiate(x_var, eo);
					multiply(mstruct);
				}
			} else if(o_function == CALCULATOR->f_tan && SIZE == 1) {
				setFunction(CALCULATOR->f_tan);
				MathStructure mstruct(CHILD(0));
				raise(2);
				add(nr_one);
				mstruct.differentiate(x_var, eo);
				multiply(mstruct, true);
				if(CALCULATOR->getRadUnit()) divide(CALCULATOR->getRadUnit());
			} else if(o_function == CALCULATOR->f_sinh && SIZE == 1) {
				setFunction(CALCULATOR->f_cosh);
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_cosh && SIZE == 1) {
				setFunction(CALCULATOR->f_sinh);
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				multiply(mstruct, true);
			} else if(o_function == CALCULATOR->f_tanh && SIZE == 1) {
				setFunction(CALCULATOR->f_tanh);
				MathStructure mstruct(CHILD(0));
				raise(2);
				negate();
				add(nr_one);
				mstruct.differentiate(x_var, eo);
				multiply(mstruct, true);
			} else if(o_function == CALCULATOR->f_asin && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				raise(2);
				negate();
				add(m_one);
				raise(Number(-1, 2));
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_acos && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				raise(2);
				negate();
				add(m_one);
				raise(Number(-1, 2));
				negate();
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_atan && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				raise(2);
				add(m_one);
				raise(m_minus_one);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_asinh && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				raise(2);
				add(m_one);
				raise(Number(-1, 2));
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_acosh && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				MathStructure mfac(*this);
				add(m_minus_one);
				raise(Number(-1, 2));
				mfac.add(m_one);
				mfac.raise(Number(-1, 2));
				multiply(mfac);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_atanh && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				SET_CHILD_MAP(0);
				raise(2);
				negate();
				add(m_one);
				raise(m_minus_one);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_sinc && SIZE == 1) {
				// diff(x)*(cos(x)/x-sin(x)/x^2)
				MathStructure m_cos(CALCULATOR->f_cos, &CHILD(0), NULL);
				if(CALCULATOR->getRadUnit()) m_cos[0].multiply(CALCULATOR->getRadUnit());
				m_cos.divide(CHILD(0));
				MathStructure m_sin(CALCULATOR->f_sin, &CHILD(0), NULL);
				if(CALCULATOR->getRadUnit()) m_sin[0].multiply(CALCULATOR->getRadUnit());
				MathStructure mstruct(CHILD(0));
				mstruct.raise(Number(-2, 1, 0));
				m_sin.multiply(mstruct);
				MathStructure mdiff(CHILD(0));
				mdiff.differentiate(x_var, eo);
				set(m_sin);
				negate();
				add(m_cos);
				multiply(mdiff);
			} else if(o_function == CALCULATOR->f_integrate && CHILD(1) == x_var && (SIZE == 2 || (SIZE == 4 && CHILD(2).isUndefined() && CHILD(3).isUndefined()))) {
				SET_CHILD_MAP(0);
			} else if(o_function == CALCULATOR->f_diff && SIZE == 3 && CHILD(1) == x_var) {
				CHILD(2) += m_one;
			} else if(o_function == CALCULATOR->f_diff && SIZE == 2 && CHILD(1) == x_var) {
				APPEND(MathStructure(2, 1, 0));
			} else if(o_function == CALCULATOR->f_diff && SIZE == 1 && x_var == (Variable*) CALCULATOR->v_x) {
				APPEND(x_var);
				APPEND(MathStructure(2, 1, 0));
			} else {
				if(!eo.calculate_functions || !calculateFunctions(eo)) {
					MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
					set(mstruct);
					return false;
				} else {
					EvaluationOptions eo2 = eo;
					eo2.calculate_functions = false;
					return differentiate(x_var, eo2);
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			MathStructure mstruct, vstruct;
			size_t idiv = 0;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isPower() && CHILD(i)[1].isNumber() && CHILD(i)[1].number().isNegative()) {
					if(idiv == 0) {
						if(CHILD(i)[1].isMinusOne()) {
							vstruct = CHILD(i)[0];
						} else {
							vstruct = CHILD(i);
							vstruct[1].number().negate();
						}
					} else {
						if(CHILD(i)[1].isMinusOne()) {
							vstruct.multiply(CHILD(i)[0], true);
						} else {
							vstruct.multiply(CHILD(i), true);
							vstruct[vstruct.size() - 1][1].number().negate();
						}
					}
					idiv++;
				}
			}
			if(idiv == SIZE) {
				set(-1, 1);
				MathStructure mdiv(vstruct);
				mdiv ^= MathStructure(2, 1, 0);
				mdiv.inverse();
				multiply(mdiv);
				MathStructure diff_u(vstruct);
				diff_u.differentiate(x_var, eo);
				multiply(diff_u, true);
				break;
			} else if(idiv > 0) {
				idiv = 0;
				for(size_t i = 0; i < SIZE; i++) {
					if(!CHILD(i).isPower() || !CHILD(i)[1].isNumber() || !CHILD(i)[1].number().isNegative()) {
						if(idiv == 0) {
							mstruct = CHILD(i);
						} else {
							mstruct.multiply(CHILD(i), true);
						}
						idiv++;
					}
				}
				MathStructure mdiv(vstruct);
				mdiv ^= MathStructure(2, 1, 0);
				MathStructure diff_v(vstruct);
				diff_v.differentiate(x_var, eo);
				MathStructure diff_u(mstruct);
				diff_u.differentiate(x_var, eo);
				vstruct *= diff_u;
				mstruct *= diff_v;
				set_nocopy(vstruct, true);
				subtract(mstruct);
				divide(mdiv);
				break;
			}
			if(SIZE > 2) {
				mstruct.set(*this);
				mstruct.delChild(mstruct.size());
				MathStructure mstruct2(LAST);
				MathStructure mstruct3(mstruct);
				mstruct.differentiate(x_var, eo);
				mstruct2.differentiate(x_var, eo);			
				mstruct *= LAST;
				mstruct2 *= mstruct3;
				set(mstruct, true);
				add(mstruct2);
			} else {
				mstruct = CHILD(0);
				MathStructure mstruct2(CHILD(1));
				mstruct.differentiate(x_var, eo);
				mstruct2.differentiate(x_var, eo);	
				mstruct *= CHILD(1);
				mstruct2 *= CHILD(0);
				set(mstruct, true);
				add(mstruct2);
			}
			break;
		}
		case STRUCT_SYMBOLIC: {
			if(representsNumber(true)) {
				clear(true);
			} else {
				MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
				set(mstruct);
				return false;
			}
			break;
		}
		case STRUCT_VARIABLE: {
			if(eo.calculate_variables && o_variable->isKnown()) {
				if(eo.approximation != APPROXIMATION_EXACT || !o_variable->isApproximate()) {
					set(((KnownVariable*) o_variable)->get(), true);
					return differentiate(x_var, eo);
				} else if(containsRepresentativeOf(x_var, true, true) != 0) {
					MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
					set(mstruct);
					return false;
				}
			}
			if(representsNumber(true)) {
				clear(true);
				break;
			}
		}		
		default: {
			MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
			set(mstruct);
			return false;
		}	
	}
	remove_zero_mul(*this);
	return true;
}

bool integrate_info(const MathStructure &mstruct, const MathStructure &x_var, MathStructure &madd, MathStructure &mmul, MathStructure &mexp, bool radunit = false, bool mexp_as_x2 = false, bool mexp_as_fx = false) {
	if(radunit && mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[1] == CALCULATOR->getRadUnit()) return integrate_info(mstruct[0], x_var, madd, mmul, mexp, false, mexp_as_x2, mexp_as_fx);
	madd.clear();
	if(mexp_as_x2 || mexp_as_fx) mexp = m_zero;
	else mexp = m_one;
	mmul = m_zero;
	if(!mexp_as_fx && mstruct == x_var) {
		if(radunit) return false;
		mmul = m_one;
		return true;
	} else if(mexp_as_x2 && mstruct.isPower()) {
		if(radunit) return false;
		if(mstruct[1].isNumber() && mstruct[1].number().isTwo() && mstruct[0] == x_var) {
			mexp = m_one;
			return true;
		}
	} else if(!mexp_as_fx && !mexp_as_x2 && mstruct.isPower() && mstruct[1].containsRepresentativeOf(x_var, true, true) == 0) {
		if(radunit) return false;
		if(mstruct[0] == x_var) {
			mexp = mstruct[1];
			mmul = m_one;
			return true;
		}
	} else if(mstruct.isMultiplication() && mstruct.size() >= 2) {
		bool b_x = false;
		bool b_rad = false;
		bool b2 = false;
		size_t i_x = 0, i_rad = 0;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!b_x && !mexp_as_fx && mstruct[i] == x_var) {
				b_x = true;
				i_x = i;
			} else if(!b_x && mexp_as_x2 && mstruct[i].isPower() && mstruct[i][1].isNumber() && mstruct[i][1].number().isTwo() && mstruct[i][0] == x_var) {
				b_x = true;
				b2 = true;
				i_x = i;
			} else if(!b_x && !mexp_as_fx && !mexp_as_x2 && mstruct[i].isPower() && mstruct[i][0] == x_var && mstruct[i][1].containsRepresentativeOf(x_var, true, true) == 0) {
				b_x = true;
				i_x = i;
				mexp = mstruct[i][1];
			} else if(mstruct[i].containsRepresentativeOf(x_var, true, true) != 0) {
				if(!b_x && mexp_as_fx && (mexp.isZero() || mexp == mstruct[i])) {
					mexp = mstruct[i];
					b_x = true;
					i_x = i;
				} else {
					return false;
				}
			} else if(!b_rad && radunit && mstruct[i] == CALCULATOR->getRadUnit()) {
				b_rad = true;
				i_rad = i;
			}
		}
		if(!b_x || (radunit && !b_rad)) return false;
		if(mstruct.size() == 1 || (radunit && mstruct.size() == 2)) {
			if(b2) mexp = m_one;
			else mmul = m_one;
		} else if(mstruct.size() == 2) {
			if(b2) {
				if(i_x == 1) mexp = mstruct[0];
				else mexp = mstruct[1];
			} else {
				if(i_x == 1) mmul = mstruct[0];
				else mmul = mstruct[1];
			}
		} else if(radunit && mstruct.size() == 3) {
			if((i_x == 1 && i_rad == 2) || (i_x == 2 && i_rad == 1)) {
				if(b2) mexp = mstruct[0];
				else mmul = mstruct[0];
			} else if((i_x == 0 && i_rad == 2) || (i_x == 2 && i_rad == 0)) {
				if(b2) mexp = mstruct[1];
				else mmul = mstruct[1];
			} else {
				if(b2) mexp = mstruct[2];
				else mmul = mstruct[2];
			}
		} else {
			if(b2) {
				mexp = mstruct;
				mexp.delChild(i_x + 1, true);
				if(radunit) {
					mexp.delChild(i_rad < i_x ? i_rad + 1 : i_rad, true);  
				}
			} else {
				mmul = mstruct;
				mmul.delChild(i_x + 1, true);
				if(radunit) {
					mmul.delChild(i_rad < i_x ? i_rad + 1 : i_rad, true);  
				}
			}
		}
		return true;
	} else if(mstruct.isAddition()) {
		mmul.setType(STRUCT_ADDITION);
		if(mexp_as_x2) mexp.setType(STRUCT_ADDITION);
		madd.setType(STRUCT_ADDITION);
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!mexp_as_fx && mstruct[i] == x_var) {
				if(mexp_as_x2 || mexp.isOne()) mmul.addChild(m_one);
				else return false;
			} else if(mexp_as_x2 && mstruct[i].isPower() && mstruct[i][1].isNumber() && mstruct[i][1].number().isTwo() && mstruct[i][0] == x_var) {
				mexp.addChild(m_one);
			} else if(!mexp_as_fx && !mexp_as_x2 && mstruct[i].isPower() && mstruct[i][0] == x_var && mstruct[i][1].containsRepresentativeOf(x_var, true, true) == 0) {
				if(mmul.size() == 0) {
					mexp = mstruct[i][1];
				} else if(mexp != mstruct[i][1]) {
					return false;
				}
				mmul.addChild(m_one);
			} else if(mstruct[i].isMultiplication()) {
				bool b_x = false;
				bool b_rad = false;
				bool b2 = false;
				size_t i_x = 0, i_rad = 0;
				for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
					if(!b_x && !mexp_as_fx && mstruct[i][i2] == x_var) {
						if(!mexp_as_x2 && !mexp.isOne()) return false;
						i_x = i2;
						b_x = true;
					} else if(!b_x && mexp_as_x2 && mstruct[i][i2].isPower() && mstruct[i][i2][1].isNumber() && mstruct[i][i2][1].number().isTwo() && mstruct[i][i2][0] == x_var) {
						b2 = true;
						i_x = i2;
						b_x = true;
					} else if(!b_x && !mexp_as_fx && !mexp_as_x2 && mstruct[i][i2].isPower() && mstruct[i][i2][0] == x_var && mstruct[i][i2][1].containsRepresentativeOf(x_var, true, true) == 0) {
						if(mmul.size() == 0) {
							mexp = mstruct[i][i2][1];
						} else if(mexp != mstruct[i][i2][1]) {
							return false;
						}
						i_x = i2;
						b_x = true;
					} else if(mstruct[i][i2].containsRepresentativeOf(x_var, true, true) != 0) {
						if(!b_x && mexp_as_fx && (mexp.isZero() || mexp == mstruct[i][i2])) {
							mexp = mstruct[i][i2];
							i_x = i2;
							b_x = true;
						} else {
							return false;
						}
					} else if(!b_rad && radunit && mstruct[i][i2] == CALCULATOR->getRadUnit()) {
						b_rad = true;
						i_rad = i2;
					}
				}
				if(radunit && !b_rad) return false;
				if(b_x) {
					if(mstruct[i].size() == 1) {
						if(b2) mexp.addChild(m_one);
						else mmul.addChild(m_one);
					} else if(radunit && mstruct[i].size() == 2) {
						if(b2) mexp.addChild(m_one);
						else mmul.addChild(m_one);
					} else {
						if(b2) {
							mexp.addChild(mstruct[i]);
							mexp[mexp.size() - 1].delChild(i_x + 1, true);
							if(radunit) mexp[mexp.size() - 1].delChild(i_rad < i_x ? i_rad + 1 : i_rad, true);
							mexp.childUpdated(mexp.size());
						} else {
							mmul.addChild(mstruct[i]);
							mmul[mmul.size() - 1].delChild(i_x + 1, true);
							if(radunit) mmul[mmul.size() - 1].delChild(i_rad < i_x ? i_rad + 1 : i_rad, true);
							mmul.childUpdated(mmul.size());
						}
					}
				} else {
					madd.addChild(mstruct[i]);
					if(radunit) {
						madd[madd.size() - 1].delChild(i_rad + 1, true);
					}
				}
			} else if(radunit && mstruct[i] == CALCULATOR->getRadUnit()) {
				madd.addChild(mstruct[i]);
			} else if(radunit || mstruct[i].containsRepresentativeOf(x_var, true, true) != 0) {
				if(!radunit && mexp_as_fx && (mexp.isZero() || mexp == mstruct[i])) {
					mexp = mstruct[i];
					mmul.addChild(m_one);
				} else {
					return false;
				}
			} else {
				madd.addChild(mstruct[i]);
			}
		}
		if(mmul.size() == 0 && (!mexp_as_x2 || mexp.size() == 0)) {
			mmul.clear();
			if(mexp_as_x2) mexp.clear();
			return false;
		}
		if(mmul.size() == 0) mmul.clear();
		else if(mmul.size() == 1) mmul.setToChild(1);
		if(mexp_as_x2) {
			if(mexp.size() == 0) mexp.clear();
			else if(mexp.size() == 1) mexp.setToChild(1);
		}
		if(madd.size() == 0) madd.clear();
		else if(madd.size() == 1) madd.setToChild(1);
		return true;
	} else if(!radunit && mexp_as_fx && mstruct.contains(x_var, true)) {
		mexp = mstruct;
		mmul = m_one;
		return true;
	}
	return false;
}

bool transform_absln(MathStructure &mstruct, int use_abs, bool definite_integral, const MathStructure &x_var, const EvaluationOptions &eo) {
	if(use_abs != 0 && mstruct.representsNonComplex(true)) {
		if(mstruct.representsNonPositive(true)) mstruct.negate();
		else if(!mstruct.representsNonNegative(true)) mstruct.transform(CALCULATOR->f_abs);
		mstruct.transform(CALCULATOR->f_ln);
	} else if(use_abs != 0 && !mstruct.representsComplex(true)) {
		if(definite_integral) use_abs = -1;
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mtest(mstruct);
		EvaluationOptions eo2 = eo;
		eo2.expand = true;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mtest.eval(eo2);
		CALCULATOR->endTemporaryStopMessages();
		if(mtest.representsNonComplex(true)) {
			if(!mtest.representsNonNegative(true)) mstruct.transform(CALCULATOR->f_abs);
			mstruct.transform(CALCULATOR->f_ln);
		} else if(mtest.representsComplex(true)) {
			mstruct.transform(CALCULATOR->f_ln);
		} else {
			if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
				CALCULATOR->beginTemporaryStopMessages();
				KnownVariable *var = new KnownVariable("", x_var.print(), ((UnknownVariable*) x_var.variable())->interval());
				mtest.replace(x_var, var);
				EvaluationOptions eo2 = eo;
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				mtest.eval(eo2);
				CALCULATOR->endTemporaryStopMessages();
				if((use_abs > 0 && !mtest.representsComplex(true)) || (use_abs < 0 && mtest.representsNonComplex(true))) {
					if(use_abs > 0 && !mtest.representsNonComplex(true)) CALCULATOR->error(false, "Integral assumed real", NULL);
					if(!mtest.representsNonNegative(true)) mstruct.transform(CALCULATOR->f_abs);
					mstruct.transform(CALCULATOR->f_ln);
				} else if(use_abs < 0 && !mtest.representsComplex(true)) {
					MathStructure marg(CALCULATOR->f_arg, &mstruct, NULL);
					marg *= nr_one_i;
					mstruct.transform(CALCULATOR->f_abs);
					mstruct.transform(CALCULATOR->f_ln);
					mstruct += marg;
				} else {
					mstruct.transform(CALCULATOR->f_ln);
				}
				var->destroy();
			} else if(use_abs < 0) {
				MathStructure marg(CALCULATOR->f_arg, &mstruct, NULL);
				marg *= nr_one_i;
				mstruct.transform(CALCULATOR->f_abs);
				mstruct.transform(CALCULATOR->f_ln);
				mstruct += marg;
			} else {
				CALCULATOR->error(false, "Integral assumed real", NULL);
				mstruct.transform(CALCULATOR->f_abs);
				mstruct.transform(CALCULATOR->f_ln);
			}
		}
	} else {
		mstruct.transform(CALCULATOR->f_ln);
	}
	return true;
}
bool test_real(MathStructure &mstruct, int use_abs, bool definite_integral, const MathStructure &x_var, const EvaluationOptions &eo) {
	if(use_abs != 0 && mstruct.representsNonComplex(true)) {
		return true;
	} else if(use_abs != 0 && !mstruct.representsComplex(true)) {
		MathStructure m_interval(m_undefined);
		if(x_var.isVariable() && !x_var.variable()->isKnown()) m_interval = ((UnknownVariable*) x_var.variable())->interval();
		if(definite_integral) use_abs = -1;
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mtest(mstruct);
		EvaluationOptions eo2 = eo;
		eo2.expand = true;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mtest.eval(eo2);
		CALCULATOR->endTemporaryStopMessages();
		if(mtest.representsNonComplex(true)) {
			return true;
		} else if(mtest.representsComplex(true)) {
			return false;
		} else {
			if(!m_interval.isUndefined()) {
				CALCULATOR->beginTemporaryStopMessages();
				mtest.replace(x_var, m_interval);
				EvaluationOptions eo2 = eo;
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				mtest.eval(eo2);
				CALCULATOR->endTemporaryStopMessages();
				if((use_abs > 0 && !mtest.representsComplex(true)) || (use_abs < 0 && mtest.representsNonComplex(true))) {
					if(use_abs > 0 && !mtest.representsNonComplex(true)) CALCULATOR->error(false, "Integral assumed real", NULL);
					return true;
				} else {
					return false;
				}
			} else if(use_abs < 0) {
				return false;
			} else {
				CALCULATOR->error(false, "Integral assumed real", NULL);
				return true;
			}
		}
	}
	return false;
}

int integrate_function(MathStructure &mstruct, const MathStructure &x_var, const EvaluationOptions &eo, const MathStructure &mpow, const MathStructure &mfac, int use_abs, bool definite_integral, int max_part_depth, vector<MathStructure*> *parent_parts) {
	if(mstruct.function() == CALCULATOR->f_ln && mstruct.size() == 1) {
		if(mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_abs && mstruct[0].size() == 1 && mpow.isOne() && mfac.isOne() && mstruct[0][0].representsNonComplex(true)) {
			MathStructure mexp, mmul, madd;
			if(integrate_info(mstruct[0][0], x_var, madd, mmul, mexp) && mexp.isOne()) {
				MathStructure marg(mstruct[0][0]);
				MathStructure ln_x(CALCULATOR->f_ln, &marg, NULL);
				MathStructure ln_mx(ln_x);
				ln_mx[0].negate();
				MathStructure sgn_x(ln_x);
				sgn_x.setFunction(CALCULATOR->f_signum);
				sgn_x.addChild(m_zero);
				mstruct = ln_x;
				mstruct -= ln_mx;
				mstruct *= sgn_x;
				mstruct += ln_mx;
				mstruct += ln_x;
				mstruct -= nr_two;
				mstruct *= marg;
				mstruct *= nr_half;
				if(!mmul.isOne()) mstruct /= mmul;
				return true;
			}
		}
		MathStructure mexp, mmul, madd;
		if(integrate_info(mstruct[0], x_var, madd, mmul, mexp)) {
			if(mfac.isOne() && (mexp.isOne() || madd.isZero())) {
				if(mpow.isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1)) {
					if(mpow.isOne()) {
						if(madd.isZero()) {
							mstruct -= mexp;
							mstruct.multiply(x_var);
							return true;
						} else {
							mstruct.multiply(mstruct[0]);
							MathStructure mmulx(mmul);
							mmulx *= x_var;
							mstruct.subtract(mmulx);
							mstruct.divide(mmul);
							return true;
						}
					} else if(mpow.number().isTwo()) {
						MathStructure marg(mstruct[0]);
						MathStructure mterm2(mstruct);
						mstruct ^= mpow;
						mterm2 *= Number(-2, 1);
						if(mexp.isOne()) {
							mterm2 += nr_two;
						} else {
							mterm2 *= mexp;
							mterm2 += mexp;
							mterm2.last() ^= nr_two;
							mterm2.last() *= nr_two;
						}
						mstruct += mterm2;
						if(madd.isZero()) {
							mstruct.multiply(x_var);
						} else {
							mstruct *= marg;
							if(!mmul.isOne()) mstruct /= mmul;
						}
						return true;
					} else if(mpow.number().isMinusOne()) {
						if(mexp.isOne()) {
							mstruct.setFunction(CALCULATOR->f_li);
							if(!mmul.isOne()) mstruct /= mmul;
							return true;
						}
					} else {
						unsigned long int n = mpow.number().uintValue();
						MathStructure marg(mstruct[0]);
						Number nfac(mpow.number());
						nfac.factorial();
						for(size_t i = 0; i <= n; i++) {
							MathStructure mterm(CALCULATOR->f_ln, &marg, NULL);
							mterm ^= Number(i);
							mterm *= nfac;
							Number ifac(i);
							ifac.factorial();
							mterm /= ifac;
							MathStructure m1pow(mexp);
							m1pow.negate();
							m1pow ^= Number(n - i);
							mterm *= m1pow;
							if(i == 0) mstruct = mterm;
							else mstruct += mterm;
						}
						if(madd.isZero()) {
							mstruct.multiply(x_var);
						} else {
							mstruct *= marg;
							if(!mmul.isOne()) mstruct /= mmul;
						}
						return true;
					}
				}
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mfacexp.isMinusOne()) {
					if(mpow.isMinusOne()) {
						if(mexp.isOne() && madd.isZero()) {
							mstruct.transform(CALCULATOR->f_ln);
							return true;
						}
					} else if(madd.isZero()) {
						MathStructure mpowp1(mpow);
						mpowp1 += m_one;
						mstruct ^= mpowp1;
						mstruct /= mpowp1;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					} else if(mpow.isOne()) {
						MathStructure m_axcb(x_var);
						if(!mexp.isOne()) m_axcb ^= mexp;
						if(!mmul.isOne()) m_axcb *= mmul;
						if(!madd.isZero()) m_axcb /= madd;
						mstruct *= m_axcb;
						mstruct.last().negate();
						mstruct.last().transform(CALCULATOR->f_ln);
						MathStructure mterm2(m_axcb);
						mterm2 += m_one;
						mterm2.transform(CALCULATOR->f_Li);
						mterm2.insertChild(nr_two, 1);
						mstruct += mterm2;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					}
				} else if(madd.isZero()) {
					if(mpow.isOne()) {
						mfacexp += m_one;
						mstruct *= mfacexp;
						mstruct -= mexp;
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mfacexp ^= Number(-2, 1);
						mstruct *= mfacexp;
						mstruct.childrenUpdated();
						return true;
					} else if(mpow.isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-100)) {
						if(mpow.isMinusOne()) {
							if(mexp.isOne()) {
								MathStructure mmulfac(mmul);
								if(!mmul.isOne()) {
									mmulfac *= x_var;
									mmulfac ^= mfacexp;
									mmulfac[1].negate();
									mmulfac *= x_var;
									mmulfac.last() ^= mfacexp;
									mmulfac.childrenUpdated(true);
								}
								mfacexp += m_one;
								mstruct *= mfacexp;
								mstruct.transform(CALCULATOR->f_Ei);
								if(!mmul.isOne()) {
									mstruct *= mmulfac;
									mstruct /= mmul;
								}
								return true;
							} else {
								MathStructure mepow(mstruct);
								mfacexp += m_one;
								mstruct *= mfacexp;
								mstruct /= mexp;
								mstruct.transform(CALCULATOR->f_Ei);
								mepow.negate();
								mepow += x_var;
								mepow.last().transform(CALCULATOR->f_ln);
								mepow.last() *= mexp;
								mepow *= mfacexp;
								mepow /= mexp;
								MathStructure memul(CALCULATOR->v_e);
								memul ^= mepow;
								mstruct *= memul;
								mstruct /= mexp;
								return true;
							}
						} else if(mpow.number().isNegative()) {
							if(mexp.isOne()) {
								MathStructure mpowp1(mpow);
								mpowp1 += m_one;
								MathStructure mpowp1n(mpowp1);
								mpowp1n.negate();
								MathStructure mterm(mstruct);
								mstruct ^= mpowp1;
								mstruct *= x_var;
								mstruct.last() ^= mfacexp;
								if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
								mfacexp += m_one;
								mstruct *= mfacexp;
								mstruct /= mpowp1n;
								mterm ^= mpowp1;
								mterm *= x_var;
								mterm.last() ^= mfacexp;
								mterm /= mpowp1n;
								mstruct -= mterm;
								mstruct.childrenUpdated(true);
								return true;
							}
						} else {
							MathStructure mterm(mstruct);
							mstruct ^= mpow;
							mstruct.last() += nr_minus_one;
							mstruct *= x_var;
							mstruct.last() ^= mfacexp;
							if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
							mstruct *= mpow;
							mfacexp += m_one;
							mstruct /= mfacexp;
							mstruct.negate();
							mstruct *= mexp;
							mterm ^= mpow;
							mterm *= x_var;
							mterm.last() ^= mfacexp;
							mterm /= mfacexp;
							mstruct += mterm;
							mstruct.childrenUpdated(true);
							return true;
						}
					}
				} else if(mfac == x_var && mexp.isOne()) {
					if(mpow.isOne()) {
						MathStructure mterm2(x_var);
						if(!mmul.isOne()) mterm2 *= mmul;
						mterm2 += madd;
						mterm2.last() *= Number(-2, 1);
						mterm2 *= x_var;
						if(!mmul.isOne()) mterm2 /= mmul;
						mterm2 *= Number(-1, 4);
						MathStructure marg(x_var);
						if(!mmul.isOne()) marg *= mmul;
						marg += madd;
						mstruct *= marg;
						marg.last() *= nr_minus_one;
						mstruct *= marg;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= Number(-2, 1);
						}
						mstruct *= nr_half;
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					}
				}
			} else if(mfac.isFunction() && mfac.function() == CALCULATOR->f_ln && mfac.size() == 1 && madd.isZero() && mpow.isOne()) {
				MathStructure mexp2, mmul2, madd2;
				if(integrate_info(mfac[0], x_var, madd2, mmul2, mexp2) && madd2.isZero()) {
					MathStructure mterm2(mfac);
					mterm2.negate();
					mterm2 += nr_two;
					if(mexp2.isOne()) mterm2 *= mexp2;
					if(mexp.isOne()) mterm2 *= mexp;
					mstruct *= mfac;
					mstruct.last() -= mexp2;
					mstruct += mterm2;
					mstruct *= x_var;
					return true;
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_lambert_w && mstruct.size() == 1 && mstruct[0] == x_var) {
		if(mpow.isOne() && mfac.isOne()) {
			MathStructure *mthis = new MathStructure(mstruct);
			mstruct.subtract(m_one);
			mthis->inverse();
			mstruct.add_nocopy(mthis);
			mstruct.multiply(x_var);
			return true;
		}
		return false;
	} else if(mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1) {
		if(mstruct[0].representsNonComplex(true) && mpow.representsNonComplex(true)) {
			if(mpow.isOne()) {
				MathStructure minteg(x_var);
				if(!mfac.isOne()) minteg *= mfac;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
				mstruct.setFunction(CALCULATOR->f_signum);
				mstruct.addChild(m_one);
				mstruct *= minteg;
				return true;
			}
			if(mfac.isOne()) {
				MathStructure mexp, mmul, madd;
				if(integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
					MathStructure marg(mstruct[0]);
					mstruct.setFunction(CALCULATOR->f_signum);
					mstruct.addChild(m_zero);
					MathStructure mterm2(mstruct);
					MathStructure mpowp1(mpow);
					mpowp1 += m_one;
					marg ^= mpowp1;
					mstruct += m_one;
					mstruct *= marg;
					marg[0].negate();
					mterm2 += nr_minus_one;
					mterm2 *= marg;
					mstruct += mterm2;
					if(!mmul.isOne()) mstruct /= mmul;
					mstruct /= mpowp1;
					mstruct *= nr_half;
					return true;
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2) {
		if(mstruct[0].representsNonComplex(true) && mpow.isNumber() && mpow.number().isRational() && mpow.number().isPositive()) {
			MathStructure minteg(x_var);
			if(!mfac.isOne()) {
				minteg = mfac;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
			}
			if(!mpow.number().isInteger()) {
				MathStructure mmul(-1, 1, 0);
				Number nr_frac(mpow.number());
				Number nr_int(mpow.number());
				nr_frac.frac();
				nr_int.trunc();
				mmul ^= nr_frac;
				if(nr_int.isEven()) mmul += nr_minus_one;
				else mmul += m_one;
				mstruct *= mmul;
				mstruct -= mmul[0];
				if(nr_int.isEven()) mstruct += nr_minus_one;
				else mstruct += m_one;
				if(nr_int.isEven()) mstruct.negate();
				mstruct /= nr_two;
			} else if(mpow.number().isEven()) {
				mstruct ^= nr_two;
				mstruct += nr_three;
				mstruct *= Number(1, 4);
			}
			mstruct *= minteg;
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_dirac && mstruct.size() == 1 && mstruct[0].representsNonComplex(true)) {
		MathStructure mexp, mmul, madd;
		if(mpow.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac == x_var && madd.representsNonZero()) {
				mstruct.setFunction(CALCULATOR->f_heaviside);
				if(!mmul.isOne()) {
					mmul ^= nr_two;
					mstruct /= mmul;
				}
				mstruct *= madd;
				mstruct.negate();
				return true;
			} else if(mfac.isPower() && mfac[0] == x_var && madd.representsNonZero()) {
				mstruct.setFunction(CALCULATOR->f_heaviside);
				madd.negate();
				madd ^= mfac[1];
				mstruct *= madd;
				if(mmul.isOne()) {
					mexp = mfac[1];
					mexp += m_one;
					mexp.negate();
					mmul ^= mexp;
					mstruct *= mmul;
				}
				return true;
			} else if(mfac.isOne()) {
				mstruct.setFunction(CALCULATOR->f_heaviside);
				if(!mmul.isOne()) mstruct /= mmul;
				return true;
			}
		}
		return false;
	} else if(mstruct.function() == CALCULATOR->f_heaviside && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				if(mmul.representsNonNegative()) {
					MathStructure mfacn(x_var);
					if(!madd.isZero()) {
						if(!mmul.isOne()) madd /= mmul;
						mfacn += madd;
					}
					mstruct *= mfacn;
					return true;
				} else if(mmul.representsNegative()) {
					if(madd.isZero()) {
						mstruct[0] = x_var;
						mstruct *= x_var;
						mstruct.negate();
						mstruct += x_var;
						return true;
					}
					mstruct.setToChild(1);
					mmul.negate();
					madd.negate();
					mstruct /= mmul;
					MathStructure mfacn(x_var);
					mfacn *= mmul;
					mfacn += madd;
					mfacn.transform(CALCULATOR->f_heaviside);
					mstruct *= mfacn;
					mstruct += x_var;
					return true;
				}
			}
		}
		return false;
	} else if(mstruct.function() == CALCULATOR->f_sin && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-2) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp, true)) {
			if(mfac.isOne() && mexp.isOne()) {
				if(mpow.isOne()) {
					mstruct.setFunction(CALCULATOR->f_cos);
					mstruct.multiply(m_minus_one);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else if(mpow.number().isTwo()) {
					if(!madd.isZero()) {
						mstruct[0] = x_var;
						mstruct[0] *= CALCULATOR->getRadUnit();
					}
					mstruct[0] *= nr_two;
					mstruct /= 4;
					if(madd.isZero() && !mmul.isOne()) mstruct /= mmul;
					mstruct.negate();
					MathStructure xhalf(x_var);
					xhalf *= nr_half;
					mstruct += xhalf;
					if(!madd.isZero()) {
						MathStructure marg(x_var);
						if(!mmul.isOne()) marg *= mmul;
						marg += madd;
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
				} else if(mpow.number().isMinusOne()) {
					MathStructure mcot(mstruct);
					mcot.setFunction(CALCULATOR->f_tan);
					mcot.inverse();
					mstruct.inverse();
					mstruct += mcot;
					if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
					if(!mmul.isOne()) mstruct.divide(mmul);
					mstruct.negate();
				} else if(mpow.number() == -2) {
					mstruct.setFunction(CALCULATOR->f_tan);
					mstruct.inverse();
					mstruct.negate();
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else {
					MathStructure mbak(mstruct);
					MathStructure nm1(mpow);
					nm1 += nr_minus_one;
					mstruct ^= nm1;
					MathStructure mcos(mbak);
					mcos.setFunction(CALCULATOR->f_cos);
					mstruct *= mcos;
					mstruct.negate();
					mmul *= mpow;
					mstruct /= mmul;
					MathStructure minteg(mbak);
					MathStructure nm2(mpow);
					nm2 += Number(-2, 1);
					minteg ^= nm2;
					if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
					minteg *= nm1;
					minteg /= mpow;
					mstruct += minteg;
				}
				return true;
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mfacexp.isMinusOne() && !mexp.isZero()) {
					if(madd.isZero()) {
						if(mpow.isOne()) {
							mstruct.setFunction(CALCULATOR->f_Si);
							if(!mexp.isOne()) mstruct /= mexp;
							return true;
						} else if(mpow.number().isTwo()) {
							mstruct[0] *= nr_two;
							mstruct.setFunction(CALCULATOR->f_Ci);
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct.negate();
							mstruct += x_var;
							if(transform_absln(mstruct.last(), use_abs, definite_integral, x_var, eo)) return -1;
							mstruct *= nr_half;
							return true;
						} else if(mpow.number() == 3) {
							mstruct.setFunction(CALCULATOR->f_Si);
							MathStructure mterm2(mstruct);
							mstruct[0] *= nr_three;
							mterm2 *= Number(-3, 1);
							mstruct += mterm2;
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct *= Number(-1, 4);
							return true;
						}
					} else if(mpow.isOne()) {
						MathStructure mterm2;
						mstruct = x_var;
						if(!mexp.isOne()) mstruct ^= mexp;
						if(!mmul.isOne()) mstruct *= mmul;
						if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
						mstruct.transform(CALCULATOR->f_Si);
						if(CALCULATOR->getRadUnit()) madd *= CALCULATOR->getRadUnit();
						mstruct *= MathStructure(CALCULATOR->f_cos, &madd, NULL);
						mterm2 = x_var;
						if(!mexp.isOne()) mterm2 ^= mexp;
						if(!mmul.isOne()) mterm2 *= mmul;
						if(CALCULATOR->getRadUnit()) mterm2 *= CALCULATOR->getRadUnit();
						mterm2.transform(CALCULATOR->f_Ci);
						mterm2 *= MathStructure(CALCULATOR->f_sin, &madd, NULL);
						mstruct += mterm2;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					}
				} else if(mexp.isOne() && mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_cos);
						mterm2 *= x_var;
						if(!mmul.isOne()) {
							mterm2 /= mmul;
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct -= mterm2;
						return true;
					} else if(mfacexp.isInteger() && mfacexp.number().isPositive() && mfacexp.number().isLessThan(100)) {
						mstruct.setFunction(CALCULATOR->f_cos);
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						mterm2.childUpdated(mterm2.size());
						if(!mmul.isOne()) mterm2 /= mmul;
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.childUpdated(mstruct.size());
						mstruct.last().last() += nr_minus_one;
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct *= mfacexp;
						if(!mmul.isOne()) mstruct /= mmul;
						mstruct -= mterm2;
						return true;
					} else if(madd.isZero() && mfacexp.isInteger() && mfacexp.number().isNegative() && mfacexp.number().isGreaterThan(-100)) {
						mfacexp += m_one;
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						mterm2.childUpdated(mterm2.size());
						mterm2 /= mfacexp;
						mstruct.setFunction(CALCULATOR->f_cos);
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.childUpdated(mstruct.size());
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct /= mfacexp;
						mstruct.negate();
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct += mterm2;
						return true;
					}
				} else if(mexp.isOne() && mpow.number().isTwo()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						MathStructure mterm3(x_var);
						mterm3 ^= nr_two;
						mterm3 *= Number(1, 4);
						mterm2[0] *= nr_two;
						mterm2 *= x_var;
						if(!mmul.isOne()) mterm2 /= mmul;
						mterm2 *= Number(-1, 4);
						mstruct.setFunction(CALCULATOR->f_cos);
						mstruct[0] *= nr_two;
						if(!mmul.isOne()) {
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct *= Number(-1, 8);
						mstruct += mterm2;
						mstruct += mterm3;
						mstruct.childrenUpdated(true);
						return true;
					} else if(mfacexp.number().isTwo()) {
						MathStructure mterm2(mstruct);
						MathStructure mterm3(x_var);
						mterm3 ^= nr_three;
						mterm3 *= Number(1, 6);
						mterm2[0] *= nr_two;
						MathStructure mterm21(1, 8, 0);
						if(!mmul.isOne()) {
							mterm21 *= mmul;
							mterm21.last() ^= Number(-3, 1);
						}
						MathStructure mterm22(x_var);
						mterm22 ^= 2;
						if(!mmul.isOne()) mterm22 /= mmul;
						mterm22 *= Number(-1, 4);
						mterm21 += mterm22;
						mterm2 *= mterm21;
						mstruct.setFunction(CALCULATOR->f_cos);
						mstruct[0] *= nr_two;
						mstruct *= x_var;
						if(!mmul.isOne()) {
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct *= Number(-1, 4);
						mstruct += mterm2;
						mstruct += mterm3;
						mstruct.childrenUpdated(true);
						return true;
					}
				}
			} else if(mexp.isOne() && mfac.isFunction()) {
				if(mfac.function() == CALCULATOR->f_sin && mfac.size() == 1 && mpow.isOne()) {
					MathStructure mexpf, mmulf, maddf;
					if(integrate_info(mfac[0], x_var, maddf, mmulf, mexpf, true) && mexpf.isOne() && mmul != mmulf) {
						MathStructure mterm2(mstruct);
						mterm2[0] += mfac[0];
						mstruct[0] -= mfac[0];
						MathStructure mden1(mmul);
						mden1 -= mmulf;
						mden1 *= nr_two;
						MathStructure mden2(mmul);
						mden2 += mmulf;
						mden2 *= nr_two;
						mterm2 /= mden2;
						mstruct /= mden1;
						mstruct -= mterm2;
						return true;
					}
				} else if(mfac.function() == CALCULATOR->f_cos && mfac.size() == 1) {
					if(mstruct[0] == mfac[0]) {
						UnknownVariable *var = new UnknownVariable("", mstruct.print());
						MathStructure mtest(var);
						if(!mpow.isOne()) mtest ^= mpow;
						CALCULATOR->beginTemporaryStopMessages();
						if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
							MathStructure m_interval(mstruct);
							m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
							var->setInterval(m_interval);
						} else {
							var->setInterval(mstruct);
						}
						if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							mtest.replace(var, mstruct);
							var->destroy();
							mstruct = mtest;
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
						var->destroy();
					} else if(mpow.isOne()) {
						MathStructure mexpf, mmulf, maddf;
						if(integrate_info(mfac[0], x_var, maddf, mmulf, mexpf, true) && mexpf.isOne()) {
							if(mmul != mmulf) {
								mstruct.setFunction(CALCULATOR->f_cos);
								MathStructure mterm2(mstruct);
								mterm2[0] += mfac[0];
								mstruct[0] -= mfac[0];
								MathStructure mden1(mmul);
								mden1 -= mmulf;
								mden1 *= nr_two;
								MathStructure mden2(mmul);
								mden2 += mmulf;
								mden2 *= nr_two;
								mterm2 /= mden2;
								mstruct /= mden1;
								mstruct.negate();
								mstruct -= mterm2;
								return true;
							} else if(madd == maddf) {
								mstruct ^= nr_two;
								if(!mmul.isOne()) mstruct /= mmul;
								mstruct *= nr_half;
								return true;
							} else {
								MathStructure mterm2(mfac);
								mterm2[0].add(mstruct[0]);
								mterm2.childUpdated(1);
								if(!mmul.isOne()) mterm2 /= mmul;
								mterm2 *= Number(-1, 4);
								mstruct[0] = maddf;
								mstruct[0] -= madd;
								mstruct[0] *= CALCULATOR->getRadUnit();
								mstruct.childUpdated(1);
								mstruct *= x_var;
								mstruct *= Number(-1, 2);
								mstruct += mterm2;
								return true;
							}
						}
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_cos && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-2) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp, true)) {
			if(mfac.isOne() && mexp.isOne()) {
				if(mpow.isOne()) {
					mstruct.setFunction(CALCULATOR->f_sin);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else if(mpow.number().isTwo()) {
					mstruct.setFunction(CALCULATOR->f_sin);
					if(!madd.isZero()) {
						mstruct[0] = x_var;
						mstruct[0] *= CALCULATOR->getRadUnit();
					}
					mstruct[0] *= nr_two;
					mstruct /= 4;
					if(madd.isZero() && !mmul.isOne()) mstruct /= mmul;
					MathStructure xhalf(x_var);
					xhalf *= nr_half;
					mstruct += xhalf;
					if(!madd.isZero()) {
						MathStructure marg(x_var);
						if(!mmul.isOne()) marg *= mmul;
						marg += madd;
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
				} else if(mpow.number().isMinusOne()) {
					MathStructure mtan(mstruct);
					mtan.setFunction(CALCULATOR->f_tan);
					mstruct.inverse();
					mstruct += mtan;
					if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else if(mpow.number() == -2) {
					mstruct.setFunction(CALCULATOR->f_tan);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else {
					MathStructure mbak(mstruct);
					MathStructure nm1(mpow);
					nm1 += nr_minus_one;
					mstruct ^= nm1;
					MathStructure msin(mbak);
					msin.setFunction(CALCULATOR->f_sin);
					mstruct *= msin;
					mmul *= mpow;
					mstruct /= mmul;
					MathStructure minteg(mbak);
					MathStructure nm2(mpow);
					nm2 += Number(-2, 1);
					minteg ^= nm2;
					if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
					minteg *= nm1;
					minteg /= mpow;
					mstruct += minteg;
				}
				return true;
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mfacexp.isMinusOne() && !mexp.isZero()) {
					if(madd.isZero()) {
						if(mpow.isOne()) {
							mstruct.setFunction(CALCULATOR->f_Ci);
							if(!mexp.isOne()) mstruct /= mexp;
							return true;
						} else if(mpow.number().isTwo()) {
							mstruct[0] *= nr_two;
							mstruct.setFunction(CALCULATOR->f_Ci);
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct += x_var;
							mstruct.last().transform(CALCULATOR->f_ln);
							mstruct *= nr_half;
							return true;
						} else if(mpow.number() == 3) {
							mstruct.setFunction(CALCULATOR->f_Ci);
							MathStructure mterm2(mstruct);
							mstruct[0] *= nr_three;
							mterm2 *= Number(3, 1);
							mstruct += mterm2;
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct *= Number(1, 4);
							return true;
						}
					} else if(mpow.isOne()) {
						MathStructure mterm2;
						mstruct = x_var;
						if(!mexp.isOne()) mstruct ^= mexp;
						if(!mmul.isOne()) mstruct *= mmul;
						if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
						mstruct.transform(CALCULATOR->f_Ci);
						if(CALCULATOR->getRadUnit()) madd *= CALCULATOR->getRadUnit();
						mstruct *= MathStructure(CALCULATOR->f_cos, &madd, NULL);
						mterm2 = x_var;
						if(!mexp.isOne()) mterm2 ^= mexp;
						if(!mmul.isOne()) mterm2 *= mmul;
						if(CALCULATOR->getRadUnit()) mterm2 *= CALCULATOR->getRadUnit();
						mterm2.transform(CALCULATOR->f_Si);
						mterm2 *= MathStructure(CALCULATOR->f_sin, &madd, NULL);
						mstruct -= mterm2;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					}
				} else if(mexp.isOne() && mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_sin);
						mterm2 *= x_var;
						if(!mmul.isOne()) {
							mterm2 /= mmul;
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct += mterm2;
						return true;
					} else if(mfacexp.isInteger() && mfacexp.number().isPositive() && mfacexp.number().isLessThan(100)) {
						mstruct.setFunction(CALCULATOR->f_sin);
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						if(!mmul.isOne()) mterm2 /= mmul;
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.last().last() += nr_minus_one;
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct *= mfacexp;
						if(!mmul.isOne()) mstruct /= mmul;
						mstruct.negate();
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					} else if(madd.isZero() && mfacexp.isInteger() && mfacexp.number().isNegative() && mfacexp.number().isGreaterThan(-100)) {
						mfacexp += m_one;
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						mterm2.childUpdated(mterm2.size());
						mterm2 /= mfacexp;
						mstruct.setFunction(CALCULATOR->f_sin);
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.childUpdated(mstruct.size());
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct /= mfacexp;
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct += mterm2;
						return true;
					}
				} else if(mexp.isOne() && mpow.number().isTwo()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_sin);
						MathStructure mterm3(x_var);
						mterm3 ^= nr_two;
						mterm3 *= Number(1, 4);
						mterm2[0] *= nr_two;
						mterm2 *= x_var;
						if(!mmul.isOne()) mterm2 /= mmul;
						mterm2 *= Number(1, 4);
						mstruct[0] *= nr_two;
						if(!mmul.isOne()) {
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct *= Number(1, 8);
						mstruct += mterm2;
						mstruct += mterm3;
						return true;
					} else if(mfacexp.number().isTwo()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_sin);
						MathStructure mterm3(x_var);
						mterm3 ^= nr_three;
						mterm3 *= Number(1, 6);
						mterm2[0] *= nr_two;
						MathStructure mterm21(-1, 8, 0);
						if(!mmul.isOne()) {
							mterm21 *= mmul;
							mterm21.last() ^= Number(-3, 1);
						}
						MathStructure mterm22(x_var);
						mterm22 ^= 2;
						if(!mmul.isOne()) mterm22 /= mmul;
						mterm22 *= Number(1, 4);
						mterm21 += mterm22;
						mterm2 *= mterm21;
						mstruct[0] *= nr_two;
						mstruct *= x_var;
						if(!mmul.isOne()) {
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct *= Number(1, 4);
						mstruct += mterm2;
						mstruct += mterm3;
						mstruct.childrenUpdated(true);
						return true;
					}
				}
			} else if(mexp.isOne() && mfac.isFunction()) {
				if(mfac.function() == CALCULATOR->f_cos && mfac.size() == 1 && mpow.isOne()) {
					MathStructure mexpf, mmulf, maddf;
					if(integrate_info(mfac[0], x_var, maddf, mmulf, mexpf, true) && mexpf.isOne() && mmulf != mmul) {
						mstruct.setFunction(CALCULATOR->f_sin);
						MathStructure mterm2(mstruct);
						mterm2[0] += mfac[0];
						mstruct[0] -= mfac[0];
						MathStructure mden1(mmul);
						mden1 -= mmulf;
						mden1 *= nr_two;
						MathStructure mden2(mmul);
						mden2 += mmulf;
						mden2 *= nr_two;
						mterm2 /= mden2;
						mstruct /= mden1;
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					}
				} else if(mfac.function() == CALCULATOR->f_sin && mfac.size() == 1) {
					if(mstruct[0] == mfac[0]) {
						UnknownVariable *var = new UnknownVariable("", mstruct.print());
						MathStructure mtest(var);
						if(!mpow.isOne()) mtest ^= mpow;
						mtest.negate();
						CALCULATOR->beginTemporaryStopMessages();
						if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
							MathStructure m_interval(mstruct);
							m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
							var->setInterval(m_interval);
						} else {
							var->setInterval(mstruct);
						}
						if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							mtest.replace(var, mstruct);
							var->destroy();
							mstruct = mtest;
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
						var->destroy();
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_tan && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mfac.isOne() && mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp, true) && mexp.isOne()) {
			if(mpow.isOne()) {
				mstruct.setFunction(CALCULATOR->f_cos);
				if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
				mstruct.negate();
				if(!mmul.isOne()) mstruct.divide(mmul);
			} else if(mpow.number().isMinusOne()) {
				mstruct.setFunction(CALCULATOR->f_sin);
				if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
				if(!mmul.isOne()) mstruct.divide(mmul);
			} else if(mpow.number().isTwo()) {
				MathStructure marg(x_var);
				if(!mmul.isOne()) marg *= mmul;
				marg += madd;
				mstruct -= marg;
				if(!mmul.isOne()) mstruct.divide(mmul);
			} else {
				MathStructure minteg(mstruct);
				MathStructure nm1(mpow);
				nm1 += nr_minus_one;
				mstruct ^= nm1;
				mmul *= nm1;
				mstruct /= mmul;
				MathStructure nm2(mpow);
				nm2 += Number(-2, 1);
				minteg ^= nm2;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
				mstruct -= minteg;
			}
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_asin && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				if(mpow.isOne()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					mstruct.multiply(x_var);
					MathStructure mterm(x_var);
					mterm ^= nr_two;
					if(madd.isZero() && !mmul.isOne()) {
						MathStructure mmul2(mmul);
						mmul2 ^= nr_two;
						mterm *= mmul2;
					}
					mterm.negate();
					mterm += m_one;
					mterm ^= nr_half;
					if(madd.isZero() && !mmul.isOne()) mterm /= mmul;
					mstruct.add(mterm);
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
					return true;
				}
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct[0]);
						mterm2 ^= nr_two;
						mterm2.negate();
						mterm2 += m_one;
						mterm2 ^= nr_half;
						MathStructure mfac2(x_var);
						if(!mmul.isOne()) mfac2 *= mmul;
						if(!madd.isZero()) {
							mfac2 += madd;
							mfac2.last() *= Number(-3, 1);
						}
						mterm2 *= mfac2;
						MathStructure mfac1(x_var);
						mfac1 ^= nr_two;
						if(!mmul.isOne()) {
							mfac1 *= mmul;
							mfac1.last() ^= nr_two;
						}
						mfac1 *= nr_two;
						if(!madd.isZero()) {
							mfac1 += madd;
							mfac1.last() ^= nr_two;
							mfac1.last() *= Number(-2, 1);
						}
						mfac1 += nr_minus_one;
						mstruct *= mfac1;
						mstruct += mterm2;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= Number(-2, 1);
						}
						mstruct *= Number(1, 4);
						return true;
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_acos && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				if(mpow.isOne()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					mstruct.multiply(x_var);
					MathStructure mterm(x_var);
					mterm ^= nr_two;
					if(madd.isZero() && !mmul.isOne()) {
						MathStructure mmul2(mmul);
						mmul2 ^= nr_two;
						mterm *= mmul2;
					}
					mterm.negate();
					mterm += m_one;
					mterm ^= nr_half;
					if(madd.isZero() && !mmul.isOne()) mterm /= mmul;
					mstruct.subtract(mterm);
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
					return true;
				}
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct[0]);
						MathStructure mterm3(mstruct);
						mterm2 ^= nr_two;
						mterm2.negate();
						mterm2 += m_one;
						mterm2 ^= nr_half;
						MathStructure mfac2(x_var);
						if(!mmul.isOne()) {
							mfac2 *= mmul;
						}
						mfac2.negate();
						if(!madd.isZero()) {
							mfac2 += madd;
							mfac2.last() *= nr_three;
						}
						mterm2 *= mfac2;
						mterm3.setFunction(CALCULATOR->f_asin);
						MathStructure mfac3(1, 1, 0);
						if(!madd.isZero()) {
							mfac3 += madd;
							mfac3.last() ^= nr_two;
							mfac3.last() *= nr_two;
						}
						mterm3 *= mfac3;
						mstruct *= x_var;
						mstruct.last() ^= nr_two;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= nr_two;
						}
						mstruct *= nr_two;
						mstruct += mterm2;
						mstruct += mterm3;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= Number(-2, 1);
						}
						mstruct *= Number(1, 4);
						return true;
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_atan && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isOne() && mfac.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			MathStructure marg(mstruct[0]);
			mstruct.multiply(marg);
			MathStructure mterm(marg);
			mterm ^= nr_two;
			mterm += m_one;
			if(!transform_absln(mterm, use_abs, definite_integral, x_var, eo)) return -1;
			mterm *= nr_half;
			mstruct.subtract(mterm);
			if(!mmul.isOne()) mstruct.divide(mmul);
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_sinh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp)) {
			if(mfac.isOne() && mexp.isOne()) {
				if(mpow.isOne()) {
					mstruct.setFunction(CALCULATOR->f_cosh);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else if(mpow.number().isTwo()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					mstruct[0] *= nr_two;
					mstruct /= 4;
					if(madd.isZero() && !mmul.isOne()) mstruct /= mmul;
					MathStructure xhalf(x_var);
					xhalf *= nr_half;
					mstruct -= xhalf;
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
				} else if(mpow.number().isMinusOne()) {
					mstruct.setFunction(CALCULATOR->f_tanh);
					mstruct[0] *= nr_half;
					if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else {
					MathStructure mbak(mstruct);
					MathStructure nm1(mpow);
					nm1 += nr_minus_one;
					mstruct ^= nm1;
					MathStructure mcos(mbak);
					mcos.setFunction(CALCULATOR->f_cosh);
					mstruct *= mcos;
					mmul *= mpow;
					mstruct /= mmul;
					MathStructure minteg(mbak);
					MathStructure nm2(mpow);
					nm2 += Number(-2, 1);
					minteg ^= nm2;
					if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
					minteg *= nm1;
					minteg /= mpow;
					mstruct -= minteg;
				}
				return true;
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mfacexp.isMinusOne() && !mexp.isZero()) {
					if(madd.isZero()) {
						if(mpow.isOne()) {
							mstruct.setFunction(CALCULATOR->f_Shi);
							if(!mexp.isOne()) mstruct /= mexp;
							return true;
						} else if(mpow.number().isTwo()) {
							mstruct[0] *= nr_two;
							mstruct.setFunction(CALCULATOR->f_Chi);
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct += x_var;
							mstruct.last().transform(CALCULATOR->f_ln);
							mstruct.last().negate();
							mstruct *= nr_half;
							return true;
						} else if(mpow.number() == 3) {
							mstruct.setFunction(CALCULATOR->f_Shi);
							MathStructure mterm2(mstruct);
							mstruct[0] *= nr_three;
							mterm2 *= Number(-3, 1);
							mstruct += mterm2;
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct *= Number(1, 4);
							return true;
						}
					} else if(mpow.isOne()) {
						MathStructure mterm2;
						mstruct = x_var;
						if(!mexp.isOne()) mstruct ^= mexp;
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct.transform(CALCULATOR->f_Shi);
						mstruct *= MathStructure(CALCULATOR->f_cosh, &madd, NULL);
						mterm2 = x_var;
						if(!mexp.isOne()) mterm2 ^= mexp;
						if(!mmul.isOne()) mterm2 *= mmul;
						mterm2.transform(CALCULATOR->f_Chi);
						mterm2 *= MathStructure(CALCULATOR->f_sinh, &madd, NULL);
						mstruct += mterm2;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					}
				} else if(mexp.isOne() && mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_cosh);
						mterm2 *= x_var;
						if(!mmul.isOne()) {
							mterm2 /= mmul;
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct.negate();
						mstruct += mterm2;
						return true;
					} else if(mfacexp.isInteger() && mfacexp.number().isPositive() && mfacexp.number().isLessThan(100)) {
						mstruct.setFunction(CALCULATOR->f_cosh);
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						if(!mmul.isOne()) mterm2 /= mmul;
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.last().last() += nr_minus_one;
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct *= mfacexp;
						if(!mmul.isOne()) mstruct /= mmul;
						mstruct.negate();
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					} else if(madd.isZero() && mfacexp.isInteger() && mfacexp.number().isNegative() && mfacexp.number().isGreaterThan(-100)) {
						mfacexp += m_one;
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						mterm2.childUpdated(mterm2.size());
						mterm2 /= mfacexp;
						mstruct.setFunction(CALCULATOR->f_cosh);
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.childUpdated(mstruct.size());
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct /= mfacexp;
						mstruct.negate();
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct += mterm2;
						return true;
					}
				}
			} else if(mfac.isFunction() && mexp.isOne()) {
				if(mfac.function() == CALCULATOR->f_sinh && mfac.size() == 1 && mpow.isOne()) {
					MathStructure mexpf, mmulf, maddf;
					if(integrate_info(mfac[0], x_var, maddf, mmulf, mexpf) && mexpf.isOne() && mmul != mmulf) {
						MathStructure mterm2(mstruct);
						mterm2[0] += mfac[0];
						mstruct[0] -= mfac[0];
						MathStructure mden1(mmul);
						mden1 -= mmulf;
						mden1 *= nr_two;
						MathStructure mden2(mmul);
						mden2 += mmulf;
						mden2 *= nr_two;
						mterm2 /= mden2;
						mstruct /= mden1;
						mstruct.negate();
						mstruct += mterm2;
						return true;
					}
				} else if(mfac.function() == CALCULATOR->f_cosh && mfac.size() == 1) {
					if(mstruct[0] == mfac[0]) {
						UnknownVariable *var = new UnknownVariable("", mstruct.print());
						MathStructure mtest(var);
						if(!mpow.isOne()) mtest ^= mpow;
						CALCULATOR->beginTemporaryStopMessages();
						if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
							MathStructure m_interval(mstruct);
							m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
							var->setInterval(m_interval);
						} else {
							var->setInterval(mstruct);
						}
						if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							mtest.replace(var, mstruct);
							var->destroy();
							mstruct = mtest;
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
						var->destroy();
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_cosh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero()&& integrate_info(mstruct[0], x_var, madd, mmul, mexp)) {
			if(mfac.isOne() && mexp.isOne()) {
				if(mpow.isOne()) {
					mstruct.setFunction(CALCULATOR->f_sinh);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else if(mpow.number().isTwo()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					mstruct.setFunction(CALCULATOR->f_sinh);
					mstruct[0] *= nr_two;
					mstruct /= 4;
					if(madd.isZero() && !mmul.isOne()) mstruct /= mmul;
					MathStructure xhalf(x_var);
					xhalf *= nr_half;
					mstruct += xhalf;
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
				} else if(mpow.number().isMinusOne()) {
					mstruct.setFunction(CALCULATOR->f_sinh);
					mstruct.transform(STRUCT_FUNCTION);
					mstruct.setFunction(CALCULATOR->f_atan);
					if(!mmul.isOne()) mstruct.divide(mmul);
				} else {
					MathStructure mbak(mstruct);
					MathStructure nm1(mpow);
					nm1 += nr_minus_one;
					mstruct ^= nm1;
					MathStructure msin(mbak);
					msin.setFunction(CALCULATOR->f_sinh);
					mstruct *= msin;
					mmul *= mpow;
					mstruct /= mmul;
					MathStructure minteg(mbak);
					MathStructure nm2(mpow);
					nm2 += Number(-2, 1);
					minteg ^= nm2;
					if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
					minteg *= nm1;
					minteg /= mpow;
					mstruct += minteg;
				}
				return true;
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mfacexp.isMinusOne() && !mexp.isZero()) {
					if(madd.isZero()) {
						if(mpow.isOne()) {
							mstruct.setFunction(CALCULATOR->f_Chi);
							if(!mexp.isOne()) mstruct /= mexp;
							return true;
						} else if(mpow.number().isTwo()) {
							mstruct[0] *= nr_two;
							mstruct.setFunction(CALCULATOR->f_Chi);
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct += x_var;
							mstruct.last().transform(CALCULATOR->f_ln);
							mstruct *= nr_half;
							return true;
						} else if(mpow.number() == 3) {
							mstruct.setFunction(CALCULATOR->f_Chi);
							MathStructure mterm2(mstruct);
							mstruct[0] *= nr_three;
							mterm2 *= nr_three;
							mstruct += mterm2;
							if(!mexp.isOne()) mstruct /= mexp;
							mstruct *= Number(1, 4);
							return true;
						}
					} else if(mpow.isOne()) {
						MathStructure mterm2;
						mstruct = x_var;
						if(!mexp.isOne()) mstruct ^= mexp;
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct.transform(CALCULATOR->f_Shi);
						mstruct *= MathStructure(CALCULATOR->f_sinh, &madd, NULL);
						mterm2 = x_var;
						if(!mexp.isOne()) mterm2 ^= mexp;
						if(!mmul.isOne()) mterm2 *= mmul;
						mterm2.transform(CALCULATOR->f_Chi);
						mterm2 *= MathStructure(CALCULATOR->f_cosh, &madd, NULL);
						mstruct += mterm2;
						if(!mexp.isOne()) mstruct /= mexp;
						return true;
					}
				} else if(mexp.isOne() && mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct);
						mterm2.setFunction(CALCULATOR->f_sinh);
						mterm2 *= x_var;
						if(!mmul.isOne()) {
							mterm2 /= mmul;
							mmul ^= nr_two;
							mstruct /= mmul;
						}
						mstruct.negate();
						mstruct += mterm2;
						return true;
					} else if(mfacexp.isInteger() && mfacexp.number().isPositive() && mfacexp.number().isLessThan(100)) {
						mstruct.setFunction(CALCULATOR->f_sinh);
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						if(!mmul.isOne()) mterm2 /= mmul;
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.last().last() += nr_minus_one;
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct *= mfacexp;
						if(!mmul.isOne()) mstruct /= mmul;
						mstruct.negate();
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					} else if(madd.isZero() && mfacexp.isInteger() && mfacexp.number().isNegative() && mfacexp.number().isGreaterThan(-100)) {
						mfacexp += m_one;
						MathStructure mterm2(mstruct);
						mterm2 *= x_var;
						mterm2.last() ^= mfacexp;
						mterm2.childUpdated(mterm2.size());
						mterm2 /= mfacexp;
						mstruct.setFunction(CALCULATOR->f_sinh);
						mstruct *= x_var;
						mstruct.last() ^= mfacexp;
						mstruct.childUpdated(mstruct.size());
						if(mstruct.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
						mstruct /= mfacexp;
						mstruct.negate();
						if(!mmul.isOne()) mstruct *= mmul;
						mstruct += mterm2;
						return true;
					}
				}
			} else if(mfac.isFunction() && mexp.isOne()) {
				if(mfac.function() == CALCULATOR->f_cosh && mfac.size() == 1 && !mpow.isOne()) {
					MathStructure mexpf, mmulf, maddf;
					if(integrate_info(mfac[0], x_var, maddf, mmulf, mexpf) && mexpf.isOne() && mmulf != mmul) {
						mstruct.setFunction(CALCULATOR->f_sinh);
						MathStructure mterm2(mstruct);
						mterm2[0] += mfac[0];
						mstruct[0] -= mfac[0];
						MathStructure mden1(mmul);
						mden1 -= mmulf;
						mden1 *= nr_two;
						MathStructure mden2(mmul);
						mden2 += mmulf;
						mden2 *= nr_two;
						mterm2 /= mden2;
						mstruct /= mden1;
						mstruct += mterm2;
						mstruct.childrenUpdated(true);
						return true;
					}
				} else if(mfac.function() == CALCULATOR->f_sinh && mfac.size() == 1) {
					if(mstruct[0] == mfac[0]) {
						UnknownVariable *var = new UnknownVariable("", mstruct.print());
						MathStructure mtest(var);
						if(!mpow.isOne()) mtest ^= mpow;
						CALCULATOR->beginTemporaryStopMessages();
						if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
							MathStructure m_interval(mstruct);
							m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
							var->setInterval(m_interval);
						} else {
							var->setInterval(mstruct);
						}
						if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							mtest.replace(var, mstruct);
							var->destroy();
							mstruct = mtest;
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
						var->destroy();
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_tanh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mfac.isOne() && mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(10) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mpow.isOne()) {
				mstruct.setFunction(CALCULATOR->f_cosh);
				mstruct.transform(STRUCT_FUNCTION);
				mstruct.setFunction(CALCULATOR->f_ln);
				if(!mmul.isOne()) mstruct.divide(mmul);
			} else if(mpow.number().isTwo()) {
				MathStructure marg(mstruct[0]);
				if(!madd.isZero()) mstruct[0] = x_var;
				if(madd.isZero() && !mmul.isOne()) mstruct /= mmul;
				mstruct.negate();
				mstruct += x_var;
				if(!madd.isZero()) {
					mstruct.replace(x_var, marg);
					if(!mmul.isOne()) mstruct.divide(mmul);
				}
			} else if(mpow.number().isMinusOne()) {
				mstruct.setFunction(CALCULATOR->f_sinh);
				if(!transform_absln(mstruct, use_abs, definite_integral, x_var, eo)) return -1;
				if(!mmul.isOne()) mstruct.divide(mmul);
			} else {
				MathStructure minteg(mstruct);
				MathStructure nm1(mpow);
				nm1 += nr_minus_one;
				mstruct ^= nm1;
				mmul *= nm1;
				mstruct /= mmul;
				mstruct.negate();
				MathStructure nm2(mpow);
				nm2 += Number(-2, 1);
				minteg ^= nm2;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) return -1;
				mstruct += minteg;
			}
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_asinh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				if(mpow.isOne()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					mstruct.multiply(x_var);
					MathStructure mterm(x_var);
					mterm ^= nr_two;
					if(madd.isZero() && !mmul.isOne()) {
						MathStructure mmul2(mmul);
						mmul2 ^= nr_two;
						mterm *= mmul2;
					}
					mterm += m_one;
					mterm ^= nr_half;
					if(madd.isZero() && !mmul.isOne()) mterm /= mmul;
					mstruct.subtract(mterm);
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
					return true;
				}
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct[0]);
						mterm2 ^= nr_two;
						mterm2 += m_one;
						mterm2 ^= nr_half;
						MathStructure mfac2(x_var);
						if(!mmul.isOne()) mfac2 *= mmul;
						mfac2.negate();
						if(!madd.isZero()) {
							mfac2 += madd;
							mfac2.last() *= nr_three;
						}
						mterm2 *= mfac2;
						MathStructure mfac1(x_var);
						mfac1 ^= nr_two;
						if(!mmul.isOne()) {
							mfac1 *= mmul;
							mfac1.last() ^= nr_two;
						}
						mfac1 *= nr_two;
						if(!madd.isZero()) {
							mfac1 += madd;
							mfac1.last() ^= nr_two;
							mfac1.last() *= Number(-2, 1);
						}
						mfac1 += m_one;
						mstruct *= mfac1;
						mstruct += mterm2;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= Number(-2, 1);
						}
						mstruct *= Number(1, 4);
						return true;
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_acosh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isNumber() && mpow.number().isInteger() && mpow.number().isLessThanOrEqualTo(100) && mpow.number().isGreaterThanOrEqualTo(-1) && !mpow.isZero() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				if(mpow.isOne()) {
					MathStructure marg(mstruct[0]);
					if(!madd.isZero()) mstruct[0] = x_var;
					MathStructure mterm(mstruct[0]);
					MathStructure msqrt2(mstruct[0]);
					mstruct.multiply(x_var);
					mterm += m_one;
					mterm ^= nr_half;
					msqrt2 += m_minus_one;
					msqrt2 ^= nr_half;
					mterm *= msqrt2;
					if(madd.isZero() && !mmul.isOne()) {
						mterm /= mmul;
					}
					mstruct.subtract(mterm);
					if(!madd.isZero()) {
						mstruct.replace(x_var, marg);
						if(!mmul.isOne()) mstruct.divide(mmul);
					}
					return true;
				}
			} else if(mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure mfacexp(1, 1, 0);
				if(mfac != x_var) mfacexp = mfac[1];
				if(mpow.isOne()) {
					if(mfacexp.isOne()) {
						MathStructure mterm2(mstruct[0]);
						MathStructure mterm2b(mstruct[0]);
						mterm2 += nr_minus_one;
						mterm2 ^= nr_half;
						mterm2b += m_one;
						mterm2b ^= nr_half;
						mterm2 *= mterm2b;
						MathStructure mfac2(x_var);
						if(!mmul.isOne()) {
							mfac2 *= mmul;
						}
						mfac2.negate();
						if(!madd.isZero()) {
							mfac2 += madd;
							mfac2.last() *= nr_three;
						}
						mterm2 *= mfac2;
						MathStructure mfac1(x_var);
						mfac1 ^= nr_two;
						if(!mmul.isOne()) {
							mfac1 *= mmul;
							mfac1.last() ^= nr_two;
						}
						mfac1 *= nr_two;
						if(!madd.isZero()) {
							mfac1 += madd;
							mfac1.last() ^= nr_two;
							mfac1.last() *= Number(-2, 1);
						}
						mfac1 += nr_minus_one;
						mstruct *= mfac1;
						mstruct += mterm2;
						if(!mmul.isOne()) {
							mstruct *= mmul;
							mstruct.last() ^= Number(-2, 1);
						}
						mstruct *= Number(1, 4);
						return true;
					}
				}
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_atanh && mstruct.size() == 1) {
		MathStructure mexp, mmul, madd;
		if(mpow.isOne() && mfac.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			MathStructure marg(mstruct[0]);
			mstruct.multiply(marg);
			mstruct.setFunction(CALCULATOR->f_cos);
			MathStructure mterm(marg);
			mterm ^= nr_two;
			mterm.negate();
			mterm += m_one;
			if(!transform_absln(mterm, use_abs, definite_integral, x_var, eo)) return -1;
			mterm *= nr_half;
			mstruct.add(mterm);
			if(!mmul.isOne()) mstruct.divide(mmul);
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct)) {
		MathStructure madd, mmul, mexp;
		if(mpow.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				MathStructure np1(mstruct[1]);
				mstruct.multiply(mstruct[0]);
				np1.inverse();
				np1 += m_one;
				if(!mmul.isOne()) np1 *= mmul;
				mstruct.divide(np1);
				return true;
			} else if(mfac == x_var) {
				MathStructure nm1(mstruct[1]);
				nm1.inverse();
				nm1 += m_one;
				MathStructure mnum(x_var);
				mnum *= nm1;
				if(!mmul.isOne()) mnum *= mmul;
				if(!madd.isZero()) mnum -= madd;
				MathStructure mden(mstruct[1]);
				mden.inverse();
				mden += nr_two;
				mden *= nm1;
				if(!mmul.isOne()) {
					mden *= mmul;
					mden.last() ^= nr_two;
				}
				mstruct.multiply(mstruct[0]);
				mstruct *= mnum;
				mstruct /= mden;
				return true;
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_erf && mstruct.size() == 1) {
		MathStructure madd, mmul, mexp;
		if(mpow.isOne() && mfac.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			MathStructure mterm2(CALCULATOR->v_e);
			mterm2 ^= mstruct[0];
			mterm2.last() ^= nr_two;
			mterm2.last().negate();
			mterm2 *= CALCULATOR->v_pi;
			mterm2.last() ^= Number(-1, 2);
			if(!mmul.isOne()) mterm2 /= mmul;
			if(madd.isZero()) {
				mstruct *= x_var;
			} else {
				mstruct *= madd;
				if(!mmul.isOne()) {
					mstruct.last() /= mmul;
					mstruct.childrenUpdated();
				}
				mstruct.last() += x_var;
			}
			mstruct += mterm2;
			return true;
		}
	} else if(mstruct.function() == CALCULATOR->f_li && mstruct.size() == 1) {
		MathStructure madd, mmul, mexp;
		if(mpow.isOne() && integrate_info(mstruct[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			if(mfac.isOne()) {
				MathStructure mEi(mstruct);
				mstruct *= mEi[0];
				mEi.setFunction(CALCULATOR->f_ln);
				mEi *= nr_two;
				mEi.transform(CALCULATOR->f_Ei);
				mstruct -= mEi;
				if(!mmul.isOne()) mstruct /= mmul;
				return true;
			} else if(madd.isZero() && mfac.isPower() && mfac[0] == x_var && mfac[1].isMinusOne()) {
				MathStructure mln(mstruct);
				mln.setFunction(CALCULATOR->f_ln);
				mstruct *= mln;
				mstruct -= mln[0];
				return true;
			}
		}
	} else if(mstruct.function() == CALCULATOR->f_diff && mstruct.size() == 3 && mstruct[1] == x_var) {
		if(!mpow.isOne() || !mfac.isOne()) return false;
		if(mstruct[2].isOne()) {
			mstruct.setToChild(1, true);
		} else {
			mstruct[2] += m_minus_one;
		}
		return true;
	} else if(mstruct.function() == CALCULATOR->f_diff && mstruct.size() == 2 && mstruct[1] == x_var) {
		if(!mpow.isOne() || !mfac.isOne()) return false;
		mstruct.setToChild(1, true);
		return true;
	} else {
		return false;
	}
	if(mstruct.size() != 1) return false;
	bool by_parts_tested = false;
	if(mfac.isOne() && (mstruct.function() == CALCULATOR->f_ln || mstruct.function() == CALCULATOR->f_asin || mstruct.function() == CALCULATOR->f_acos || mstruct.function() == CALCULATOR->f_atan || mstruct.function() == CALCULATOR->f_asinh || mstruct.function() == CALCULATOR->f_acosh || mstruct.function() == CALCULATOR->f_atanh)) {
		by_parts_tested = true;
		//integrate by parts
		if(max_part_depth > 0) {
			MathStructure minteg(mstruct);
			minteg ^= mpow;
			CALCULATOR->beginTemporaryStopMessages();
			if(minteg.differentiate(x_var, eo) && minteg.containsFunction(CALCULATOR->f_diff, true) <= 0) {
				minteg *= x_var;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth - 1, parent_parts) > 0 && minteg.containsFunction(CALCULATOR->f_integrate, true) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct ^= mpow;
					mstruct.multiply(x_var);
					mstruct.subtract(minteg);
					return true;
				}
			}
			CALCULATOR->endTemporaryStopMessages();
		}
	}
	MathStructure madd, mmul, mexp;
	if(integrate_info(mstruct[0], x_var, madd, mmul, mexp, false, false, true) && !mexp.isZero()) {
		if(mfac.isOne() && (mexp.isPower() && mexp[0] == x_var && mexp[1].isNumber() && !mexp[1].number().isInteger() && mexp[1].number().isRational())) {
			Number num(mexp[1].number().numerator());
			Number den(mexp[1].number().denominator());
			if(num.isPositive() || num.isMinusOne()) {
				MathStructure morig(x_var);
				if(num.isNegative()) den.negate();
				Number den_inv(den);
				den_inv.recip();
				morig ^= den_inv;
				UnknownVariable *var = new UnknownVariable("", string(LEFT_PARENTHESIS) + morig.print() + RIGHT_PARENTHESIS);
				Number den_m1(den);
				den_m1--;
				MathStructure mtest(var);
				if(!num.isOne() && !num.isMinusOne()) mtest ^= num;
				if(!mmul.isOne()) mtest *= mmul;
				if(!madd.isZero()) mtest += madd;
				mtest.transform(mstruct.function());
				if(!mpow.isOne()) mtest ^= mpow;
				mtest *= var;
				if(!den_m1.isOne()) {
					mtest.last() ^= den_m1;
					mtest.childUpdated(mtest.size());
				}
				if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
					MathStructure m_interval(morig);
					m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
					var->setInterval(m_interval);
				} else {
					var->setInterval(morig);
				}
				CALCULATOR->beginTemporaryStopMessages();
				if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct.set(mtest, true);
					mstruct.replace(var, morig);
					mstruct.multiply(den);
					var->destroy();
					return true;
				}
				CALCULATOR->endTemporaryStopMessages();
				var->destroy();
			}
		} else if((mfac.isOne() || mfac == x_var || (mfac.isPower() && mfac[0] == x_var && mfac[1].isInteger())) && (mexp.isPower() && mexp[0] != x_var && mexp[1].isNumber())) {
			MathStructure madd2, mmul2, mexp2;
			if(integrate_info(mexp[0], x_var, madd2, mmul2, mexp2) && (!madd.isZero() || mexp != x_var) && mexp2.isOne()) {
				UnknownVariable *var = new UnknownVariable("", string(LEFT_PARENTHESIS) + mexp[0].print() + RIGHT_PARENTHESIS);
				if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
					MathStructure m_interval(mexp[0]);
					m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
					var->setInterval(m_interval);
				} else {
					var->setInterval(mexp[0]);
				}
				MathStructure mtest(var);
				mtest ^= mexp[1];
				if(!mmul.isOne()) mtest *= mmul;
				if(!madd.isZero()) mtest += madd;
				mtest.transform(mstruct.function());
				if(!mpow.isOne()) mtest ^= mpow;
				if(!mfac.isOne()) {
					mtest *= var;
					if(!madd2.isZero()) {
						mtest.last() -= madd2;
						mtest.childUpdated(mtest.size());
					}
					if(mfac.isPower()) {
						mtest.last() ^= mfac[1];
						mtest.childUpdated(mtest.size());
					}
				}
				CALCULATOR->beginTemporaryStopMessages();
				if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct.set(mtest, true);
					mstruct.replace(var, mexp[0]);
					if(!mmul2.isOne()) {
		 				mstruct /= mmul2;
						if(!mfac.isOne()) {
							if(mfac.isPower()) mmul2 ^= mfac[1];
							mstruct /= mmul2;
						}
					}
					var->destroy();
					return true;
				}
				CALCULATOR->endTemporaryStopMessages();
				var->destroy();
			}
		}
	}
	if(!mfac.isOne()) return false;
	MathStructure *m_func = NULL, *m_pow = NULL;
	if(mstruct[0].isFunction() && mstruct[0].contains(x_var, true) > 0) {
		m_func = &mstruct[0];
	} else if(mstruct[0].isPower() && mstruct[0][0].isFunction() && mstruct[0][0].contains(x_var, true) > 0) {
		m_func = &mstruct[0][0];
	} else if(mstruct[0].isPower() && mstruct[0][1].contains(x_var, true) > 0) {
		m_pow = &mstruct[0];
	} else if(mstruct[0].isMultiplication()) {
		for(size_t i = 0; i < mstruct[0].size(); i++) {
			if(mstruct[0][i].isFunction() && mstruct[0][i].contains(x_var, true) > 0) {
				m_func = &mstruct[0][i];
			} else if(mstruct[0][i].isPower() && mstruct[0][i][0].isFunction() && mstruct[0][i][0].contains(x_var, true) > 0) {
				m_func = &mstruct[0][i][0];
			} else if(mstruct[0][i].isPower() && mstruct[0][i][1].contains(x_var, true) > 0) {
				m_pow = &mstruct[0][i];
			}
		}
	} else if(mstruct[0].isAddition()) {
		for(size_t i2 = 0; i2 < mstruct[0].size(); i2++) {
			if(mstruct[0][i2].isFunction() && mstruct[0][i2].contains(x_var, true) > 0) {
				m_func = &mstruct[0][i2];
			} else if(mstruct[0][i2].isPower() && mstruct[0][i2][0].isFunction() && mstruct[0][i2][0].contains(x_var, true) > 0) {
				m_func = &mstruct[0][i2][0];
			} else if(mstruct[0][i2].isPower() && mstruct[0][i2][1].contains(x_var, true) > 0) {
				m_pow = &mstruct[0][i2];
			} else if(mstruct[0][i2].isMultiplication()) {
				for(size_t i = 0; i < mstruct[0][i2].size(); i++) {
					if(mstruct[0][i2][i].isFunction() && mstruct[0][i2][i].contains(x_var, true) > 0) {
						m_func = &mstruct[0][i2][i];
					} else if(mstruct[0][i2][i].isPower() && mstruct[0][i2][i][0].isFunction() && mstruct[0][i2][i][0].contains(x_var, true) > 0) {
						m_func = &mstruct[0][i2][i][0];
					} else if(mstruct[0][i2][i].isPower() && mstruct[0][i2][i][1].contains(x_var, true) > 0) {
						m_pow = &mstruct[0][i2][i];
					}
				}
			}
		}
	}
	if(m_func && m_pow) return false;
	if(m_func) {
		if((m_func->function() == CALCULATOR->f_ln || m_func->function() == CALCULATOR->f_asin || m_func->function() == CALCULATOR->f_acos || m_func->function() == CALCULATOR->f_asinh || m_func->function() == CALCULATOR->f_acosh) && m_func->size() == 1 && integrate_info((*m_func)[0], x_var, madd, mmul, mexp) && mexp.isOne()) {
			MathStructure m_orig(*m_func);
			UnknownVariable *var = new UnknownVariable("", m_orig.print());
			MathStructure mtest(mstruct);
			mtest[0].replace(m_orig, var);
			if(mtest[0].containsRepresentativeOf(x_var, true, true) == 0) {
				if(m_func->function() == CALCULATOR->f_ln) {
					MathStructure m_epow(CALCULATOR->v_e);
					m_epow ^= var;
					mtest *= m_epow;
				} else if(m_func->function() == CALCULATOR->f_asin) {
					MathStructure m_cos(var);
					if(CALCULATOR->getRadUnit()) m_cos *= CALCULATOR->getRadUnit();
					m_cos.transform(CALCULATOR->f_cos);
					mtest *= m_cos;
				} else if(m_func->function() == CALCULATOR->f_acos) {
					MathStructure m_sin(var);
					if(CALCULATOR->getRadUnit()) m_sin *= CALCULATOR->getRadUnit();
					m_sin.transform(CALCULATOR->f_sin);
					mtest *= m_sin;
					mmul.negate();
				} else if(m_func->function() == CALCULATOR->f_asinh) {
					MathStructure m_cos(var);
					m_cos.transform(CALCULATOR->f_cosh);
					mtest *= m_cos;
				} else if(m_func->function() == CALCULATOR->f_acosh) {
					MathStructure m_sin(var);
					m_sin.transform(CALCULATOR->f_sinh);
					mtest *= m_sin;
				}
				if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
					MathStructure m_interval(m_orig);
					m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
					var->setInterval(m_interval);
				} else {
					var->setInterval(m_orig);
				}
				CALCULATOR->beginTemporaryStopMessages();
				if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct.set(mtest, true);
					mstruct.replace(var, m_orig);
					if(!mmul.isOne()) mstruct /= mmul;
					var->destroy();
					return true;
				}
				CALCULATOR->endTemporaryStopMessages();
			}
			var->destroy();
		}
	}
	if(m_pow) {
		if((*m_pow)[0].containsRepresentativeOf(x_var, true, true) == 0 && integrate_info((*m_pow)[1], x_var, madd, mmul, mexp) && mexp.isOne()) {
			MathStructure m_orig(*m_pow);
			UnknownVariable *var = new UnknownVariable("", string(LEFT_PARENTHESIS) + m_orig.print() + RIGHT_PARENTHESIS);
			MathStructure mtest(mstruct);
			mtest[0].replace(m_orig, var);
			if(mtest[0].containsRepresentativeOf(x_var, true, true) == 0) {
				if(!mpow.isOne()) mtest ^= mpow;
				mtest /= var;
				if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
					MathStructure m_interval(m_orig);
					m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
					var->setInterval(m_interval);
				} else {
					var->setInterval(m_orig);
				}
				CALCULATOR->beginTemporaryStopMessages();
				if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct.set(mtest, true);
					mstruct.replace(var, m_orig);
					MathStructure m_ln(CALCULATOR->f_ln, &m_orig[0], NULL);
					mstruct /= m_ln;
					if(!mmul.isOne()) mstruct /= mmul;
					var->destroy();
					return true;
				}
				CALCULATOR->endTemporaryStopMessages();
			}
			var->destroy();
		}
	}
	if(!by_parts_tested && mstruct[0].function() != CALCULATOR->f_sin && mstruct[0].function() != CALCULATOR->f_cos && mstruct[0].function() != CALCULATOR->f_tan && mstruct[0].function() != CALCULATOR->f_sinh && mstruct[0].function() != CALCULATOR->f_cosh && mstruct[0].function() != CALCULATOR->f_tanh) {
		//integrate by parts
		if(max_part_depth > 0) {
			MathStructure minteg(mstruct);
			minteg ^= mpow;
			CALCULATOR->beginTemporaryStopMessages();
			if(minteg.differentiate(x_var, eo) && minteg.containsFunction(CALCULATOR->f_diff, true) <= 0) {
				minteg *= x_var;
				if(minteg.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth - 1, parent_parts) > 0 && minteg.containsFunction(CALCULATOR->f_integrate, true) <= 0) {
					CALCULATOR->endTemporaryStopMessages(true);
					mstruct ^= mpow;
					mstruct.multiply(x_var);
					mstruct.subtract(minteg);
					return true;
				}
			}
			CALCULATOR->endTemporaryStopMessages();
		}
	}
	return false;
}

#define CANNOT_INTEGRATE {MathStructure minteg(CALCULATOR->f_integrate, this, &x_var, &m_undefined, &m_undefined, NULL); set(minteg); return false;}
#define CANNOT_INTEGRATE_INTERVAL {MathStructure minteg(CALCULATOR->f_integrate, this, &x_var, &m_undefined, &m_undefined, NULL); set(minteg); return -1;}

bool MathStructure::decomposeFractions(const MathStructure &x_var, const EvaluationOptions &eo) {
	MathStructure mtest2;
	bool b = false;
	int mmul_i = 0;
	if(isPower()) {
		if(CHILD(1).isMinusOne() && CHILD(0).isMultiplication() && CHILD(0).size() >= 2) {
			mtest2 = CHILD(0);
			b = true;
		}
	} else if(isMultiplication() && SIZE == 2) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isPower() && CHILD(i)[1].isMinusOne() && (CHILD(i)[0].isPower() || CHILD(i)[0].isMultiplication())) {
				mtest2 = CHILD(i)[0];
				b = true;
			} else if(CHILD(i) == x_var) {
				mmul_i = 1;
			} else if(CHILD(i).isPower() && CHILD(i)[0] == x_var && CHILD(i)[1].isInteger() && CHILD(i)[1].number().isPositive() && CHILD(i)[1].number().isLessThan(100)) {
				mmul_i = CHILD(i)[1].number().intValue();
			}
		}
		if(mmul_i == 0) b = false;
	}
	if(b) {
		if(!mtest2.isMultiplication()) mtest2.transform(STRUCT_MULTIPLICATION);
		MathStructure mfacs, mnew;
		mnew.setType(STRUCT_ADDITION);
		mfacs.setType(STRUCT_ADDITION);
		vector<int> i_degrees;
		i_degrees.resize(mtest2.size(), 1);
		for(size_t i = 0; i < mtest2.size() && b; i++) {
			MathStructure mfactor = mtest2[i];
			if(mtest2[i].isPower()) {
				if(!mtest2[i][1].isInteger() || !mtest2[i][1].number().isPositive() || mtest2[i][1].isOne() || mtest2[i][1].number().isGreaterThan(100)) {
					b = false;
					break;
				}
				mfactor = mtest2[i][0];
			}
			if(mfactor.isAddition()) {
				bool b2 = false;
				for(size_t i2 = 0; i2 < mfactor.size() && b; i2++) {
					if(mfactor[i2].isMultiplication()) {
						bool b_x = false;
						for(size_t i3 = 0; i3 < mfactor[i2].size() && b; i3++) {
							if(!b_x && mfactor[i2][i3].isPower() && mfactor[i2][i3][0] == x_var) {
								if(!mfactor[i2][i3][1].isInteger() || !mfactor[i2][i3][1].number().isPositive() || mfactor[i2][i3][1].isOne() || mfactor[i2][i3][1].number().isGreaterThan(100)) {
									b = false;
								} else if(mfactor[i2][i3][1].number().intValue() > i_degrees[i]) {
									i_degrees[i] = mfactor[i2][i3][1].number().intValue();
								}
								b_x = true;
							} else if(!b_x && mfactor[i2][i3] == x_var) {
								b_x = true;
							} else if(mfactor[i2][i3].containsRepresentativeOf(x_var, true, true) != 0) {
								b = false;
							}
						}
						if(!b_x) b2 = true;
					} else if(mfactor[i2].isPower() && mfactor[i2][0] == x_var) {
						if(!mfactor[i2][1].isInteger() || !mfactor[i2][1].number().isPositive() || mfactor[i2][1].isOne() || mfactor[i2][1].number().isGreaterThan(100)) {
							b = false;
						} else if(mfactor[i2][1].number().intValue() > i_degrees[i]) {
							i_degrees[i] = mfactor[i2][1].number().intValue();
						}
					} else if(mfactor[i2] == x_var) {
					} else if(mfactor[i2].containsRepresentativeOf(x_var, true, true) != 0) {
						b = false;
					} else {
						b2 = true;
					}
				}
				if(!b2) b = false;
			} else if(mfactor != x_var) {
				b = false;
			}
		}
		MathStructure mtest3, mnums3;
		mnums3.clearVector();
		mtest3.clearVector();
		if(b) {
			UnknownVariable *var = new UnknownVariable("", string("a"));
			MathStructure mvar(var);
			for(size_t i = 0; i < mtest2.size(); i++) {
				if(i_degrees[i] == 1) {
					MathStructure mnum(1, 1, 0);
					if(mtest2.size() != 1) {
						mnum = mtest2;
						mnum.delChild(i + 1, true);
					}
					MathStructure mx(mtest2[i]);
					mx.transform(COMPARISON_EQUALS, m_zero);
					mx.replace(x_var, mvar);
					mx.isolate_x(eo, mvar);
					mx.calculatesub(eo, eo, true);
					if(mx.isLogicalOr()) mx.setToChild(1);
					if(!mx.isComparison() || mx.comparisonType() != COMPARISON_EQUALS || mx[0] != var) {b = false; break;}
					mx.setToChild(2);
					if(mtest2.size() != 1) {
						mfacs.addChild(mnum);
						mnum.replace(x_var, mx);
						mnum.inverse();
					}
					if(mmul_i > 0) {
						mx ^= Number(mmul_i, 1);
						if(mtest2.size() == 1) mnum = mx;
						else mnum *= mx;
					}
					mnum.calculatesub(eo, eo, true);
					if(mtest2.size() == 1) mfacs.addChild(mnum);
					else mfacs.last() *= mnum;
					mnums3.addChild(mnum);
					mtest3.addChild(mtest2[i]);
				}
			}
			var->destroy();
		}
		if(b) {
			vector<UnknownVariable*> vars;
			bool b_solve = false;
			for(size_t i = 0; i < mtest2.size(); i++) {
				if(mtest2[i].isPower()) {
					int i_exp = mtest2[i][1].number().intValue();
					for(int i_exp_n = mtest2[i][1].number().intValue() - (i_degrees[i] == 1 ? 1 : 0); i_exp_n > 0; i_exp_n--) {
						if(i_exp_n == 1) {
							mtest3.addChild(mtest2[i][0]);
						} else {
							mtest3.addChild(mtest2[i]);
							if(i_exp_n != i_exp) mtest3.last()[1].number() = i_exp_n;
						}
						if(i_exp ==  i_exp_n) {
							if(mtest2.size() > 1) {
								mfacs.addChild(mtest2);
								mfacs.last().delChild(i + 1, true);
							}
						} else {
							mfacs.addChild(mtest2);
							if(i_exp - i_exp_n == 1) mfacs.last()[i].setToChild(1);
							else mfacs.last()[i][1].number() = i_exp - i_exp_n;
						}
						if(i_degrees[i] == 1) {
							UnknownVariable *var = new UnknownVariable("", string("a") + i2s(mtest3.size()));
							mnums3.addChild_nocopy(new MathStructure(var));
							vars.push_back(var);
						} else {
							mnums3.addChild_nocopy(new MathStructure());
							mnums3.last().setType(STRUCT_ADDITION);
							for(int i2 = 1; i2 <= i_degrees[i]; i2++) {
								UnknownVariable *var = new UnknownVariable("", string("a") + i2s(mtest3.size()) + i2s(i2));
								if(i2 == 1) {
									mnums3.last().addChild_nocopy(new MathStructure(var));
								} else {
									mnums3.last().addChild_nocopy(new MathStructure(var));
									mnums3.last().last() *= x_var;
									if(i2 > 2) mnums3.last().last().last() ^= Number(i2 - 1, 1);
								}
								vars.push_back(var);
							}
						}
						if(i_exp != i_exp_n || mtest2.size() > 1) mfacs.last() *= mnums3.last();
						else mfacs.addChild(mnums3.last());
						b_solve = true;
					}
				} else if(i_degrees[i] > 1) {
					mtest3.addChild(mtest2[i]);
					if(mtest2.size() > 1) {
						mfacs.addChild(mtest2);
						mfacs.last().delChild(i + 1, true);
					}
					mnums3.addChild_nocopy(new MathStructure());
					mnums3.last().setType(STRUCT_ADDITION);
					for(int i2 = 1; i2 <= i_degrees[i]; i2++) {
						UnknownVariable *var = new UnknownVariable("", string("a") + i2s(mtest3.size()) + i2s(i2));
						if(i2 == 1) {
							mnums3.last().addChild_nocopy(new MathStructure(var));
						} else {
							mnums3.last().addChild_nocopy(new MathStructure(var));
							mnums3.last().last() *= x_var;
							if(i2 > 2) mnums3.last().last().last() ^= Number(i2 - 1, 1);
						}
						vars.push_back(var);
					}
					if(mtest2.size() > 1) mfacs.last() *= mnums3.last();
					else mfacs.addChild(mnums3.last());
					b_solve = true;
				}
			}
			if(b_solve) {
				mfacs.childrenUpdated(true);
				MathStructure mfac_expand(mfacs);
				mfac_expand.calculatesub(eo, eo, true);
				size_t i_degree = 0;
				if(mfac_expand.isAddition()) {
					i_degree = mfac_expand.degree(x_var).uintValue(); 
					if(i_degree >= 100 || i_degree <= 0) b = false;
				}
				if(i_degree == 0) b = false;
				if(b) {
					MathStructure m_eqs;
					m_eqs.resizeVector(i_degree + 1, m_zero);
					for(size_t i = 0; i < m_eqs.size(); i++) {
						for(size_t i2 = 0; i2 < mfac_expand.size();) {
							bool b_add = false;
							if(i == 0) {
								if(!mfac_expand[i2].contains(x_var)) b_add = true;
							} else {
								if(mfac_expand[i2].isMultiplication() && mfac_expand[i2].size() >= 2) {
									for(size_t i3 = 0; i3 < mfac_expand[i2].size(); i3++) {
										if(i == 1 && mfac_expand[i2][i3] == x_var) b_add = true;
										else if(i > 1 && mfac_expand[i2][i3].isPower() && mfac_expand[i2][i3][0] == x_var && mfac_expand[i2][i3][1] == i) b_add = true;
										if(b_add) {
											mfac_expand[i2].delChild(i3 + 1, true);
											break;
										}
									}
									
								} else {
									if(i == 1 && mfac_expand[i2] == x_var) b_add = true;
									else if(i > 1 && mfac_expand[i2].isPower() && mfac_expand[i2][0] == x_var && mfac_expand[i2][1] == i) b_add = true;
									if(b_add) mfac_expand[i2].set(1, 1, 0);
								}
							}
							if(b_add) {
								if(m_eqs[i].isZero()) m_eqs[i] = mfac_expand[i2];
								else m_eqs[i].add(mfac_expand[i2], true);
								mfac_expand.delChild(i2 + 1);
							} else {
								i2++;
							}
						}
					}
					if(mfac_expand.size() == 0 && m_eqs.size() >= vars.size()) {
						for(size_t i = 0; i < m_eqs.size(); i++) {
							if(!m_eqs[i].isZero()) {
								m_eqs[i].transform(COMPARISON_EQUALS, i == (size_t) mmul_i ? m_one : m_zero);
							}
						}
						for(size_t i = 0; i < m_eqs.size();) {
							if(m_eqs[i].isZero()) {
								m_eqs.delChild(i + 1);
								if(i == (size_t) mmul_i) b = false;
							} else {
								i++;
							}
						}
						if(b) {
							MathStructure vars_m;
							vars_m.clearVector();
							for(size_t i = 0; i < vars.size(); i++) {
								vars_m.addChild_nocopy(new MathStructure(vars[i]));
							}
							for(size_t i = 0; i < m_eqs.size() && b; i++) {
								for(size_t i2 = 0; i2 < vars_m.size(); i2++) {
									if(m_eqs[i].isComparison() && m_eqs[i][0].contains(vars_m[i2], true)) {
										m_eqs[i].isolate_x(eo, vars_m[i2]);
										m_eqs[i].calculatesub(eo, eo, true);
										if(m_eqs[i].isLogicalOr()) m_eqs[i].setToChild(1);
										if(m_eqs[i].isComparison() && m_eqs[i].comparisonType() == COMPARISON_EQUALS && m_eqs[i][0] == vars_m[i2]) {
											for(size_t i3 = 0; i3 < m_eqs.size(); i3++) {
												if(i3 != i) {
													m_eqs[i3][0].calculateReplace(vars_m[i2], m_eqs[i][1], eo);
													m_eqs[i3][1].calculateReplace(vars_m[i2], m_eqs[i][1], eo);
												}
											}
											vars_m.delChild(i2 + 1);
										} else {
											b = false;
										}
										break;
									}
								}
							}
							for(size_t i = 0; i < m_eqs.size(); i++) {
								m_eqs[i].calculatesub(eo, eo);
								if(m_eqs[i].isZero()) {b = false; break;}
							}
							if(b && vars_m.size() == 0) {
								for(size_t i = 0; i < vars.size(); i++) {
									for(size_t i2 = 0; i2 < m_eqs.size(); i2++) {
										if(m_eqs[i2][0] == vars[i]) {
											mnums3.replace(vars[i], m_eqs[i2][1]);
											break;
										}
									}
								}
							} else {
								b = false;
							}
						}
					} else {
						b = false;
					}
				}
			}
			for(size_t i = 0; i < vars.size(); i++) {
				vars[i]->destroy();
			}
			if(b) {
				for(size_t i = 0; i < mnums3.size(); i++) {
					mnums3.calculatesub(eo, eo, true);
					if(!mnums3[i].isZero()) {
						if(mnums3[i].isOne()) {
							mnew.addChild(mtest3[i]);
							mnew.last().inverse();
						} else {
							mnew.addChild(mnums3[i]);
							mnew.last() /= mtest3[i];
						}
					}
				}
			}
			if(b) {
				if(mnew.size() == 0) mnew.clear();
				else if(mnew.size() == 1) mnew.setToChild(1);
				mnew.childrenUpdated();
				if(equals(mnew, true)) return false;
				set(mnew, true);
				return true;
			}
		}
	}
	return false;
}

int contains_unsolved_integrate(const MathStructure &mstruct, MathStructure *this_mstruct, MathStructure *parent_parts);
int contains_unsolved_integrate(const MathStructure &mstruct, MathStructure *this_mstruct, vector<MathStructure*> *parent_parts) {
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_integrate) {
		if(this_mstruct->equals(mstruct[0], true)) return 3;
		for(size_t i = 0; i < parent_parts->size(); i++) {
			if(mstruct[0].equals(*(*parent_parts)[i], true)) return 2;
		}
		return 1;
	}
	int ret = 0;
	for(size_t i = 0; i < mstruct.size(); i++) {
		int ret_i = contains_unsolved_integrate(mstruct[i], this_mstruct, parent_parts);
		if(ret_i == 1) {
			return 1;
		} else if(ret_i > ret) {
			ret = ret_i;
		}
	}
	return ret;
}

bool fix_abs_x(MathStructure &mstruct, const MathStructure &x_var) {
	bool b = false;
	if(mstruct.isFunction() && mstruct.size() == 1 && mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_abs && mstruct[0].size() == 1 && mstruct[0][0].representsNonComplex(true)) {
		if(mstruct.function() == CALCULATOR->f_sin || mstruct.function() == CALCULATOR->f_tan || mstruct.function() == CALCULATOR->f_sinh || mstruct.function() == CALCULATOR->f_tanh || mstruct.function() == CALCULATOR->f_asin || mstruct.function() == CALCULATOR->f_atan || mstruct.function() == CALCULATOR->f_asinh || mstruct.function() == CALCULATOR->f_atanh) {
			mstruct[0].setToChild(1, true);
			mstruct.multiply_nocopy(new MathStructure(CALCULATOR->f_signum, &mstruct[0], &m_zero, NULL));
			mstruct.evalSort(false);
			b = true;
		} else if(mstruct.function() == CALCULATOR->f_cos || mstruct.function() == CALCULATOR->f_cosh) {
			mstruct[0].setToChild(1, true);
			b = true;
		}
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(fix_abs_x(mstruct[i], x_var)) b = true;
	}
	return b;
}
MathStructure *find_abs_x(MathStructure &mstruct, const MathStructure &x_var) {
	if(mstruct.isFunction() && ((mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1) || (mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct) && mstruct[1].number().isOdd())) && mstruct[0].contains(x_var, true) > 0 && mstruct[0].representsNonComplex(true)) {
		return &mstruct[0];
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		MathStructure *m = find_abs_x(mstruct[i], x_var);
		if(m) return m;
	}
	return NULL;
}
bool fix_sgn_x(MathStructure &mstruct, const MathStructure &x_var, const EvaluationOptions &eo) {
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2) {
		MathStructure mtest(mstruct);
		KnownVariable *var = new KnownVariable("", x_var.print(), ((UnknownVariable*) x_var.variable())->interval());
		mtest.replace(x_var, var);
		CALCULATOR->beginTemporaryStopMessages();
		mtest.eval(eo);
		var->destroy();
		if(!CALCULATOR->endTemporaryStopMessages() && !mtest.isFunction()) {
			mstruct.set(mtest);
			return true;
		}
	}
	bool b = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(fix_sgn_x(mstruct[i], x_var, eo)) b = true;
	}
	return b;
}
bool replace_abs_x(MathStructure &mstruct, const MathStructure &marg, bool b_minus) {
	if(mstruct.isFunction()) {
		if(mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && mstruct[0] == marg) {
			mstruct.setToChild(1);
			if(b_minus) mstruct.negate();
			return true;
		} else if(mstruct.function() == CALCULATOR->f_root && VALID_ROOT(mstruct) && mstruct[1].number().isOdd() && mstruct[0] == marg) {
			if(b_minus) mstruct[0].negate();
			mstruct[1].number().recip();
			mstruct.setType(STRUCT_POWER);
			mstruct.childrenUpdated();
			if(b_minus) mstruct.negate();
			return true;
		}
	}
	if(mstruct.isPower() && mstruct[1].isInteger() && mstruct[1].number().isOdd() && mstruct[0].isFunction() && mstruct[0].function() == CALCULATOR->f_root && VALID_ROOT(mstruct[0]) && mstruct[0][1].number().isOdd() && mstruct[0][0] == marg) {
		mstruct[1].number().divide(mstruct[0][1].number());
		mstruct[0].setToChild(1, true);
		if(mstruct[1].number().isOne()) mstruct.setToChild(1, true);
		if(b_minus) mstruct[0].negate();
		mstruct.childrenUpdated();
		if(b_minus) mstruct.negate();
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(replace_abs_x(mstruct[i], marg, b_minus)) {
			mstruct.childUpdated(i + 1);
			b = true;
		}
	}
	return b;
}

bool test_sign_zero(const MathStructure &m, const MathStructure &x_var, const MathStructure &mzero, const EvaluationOptions &eo) {
	if(m.contains(x_var, true) <= 0) return false;
	if(m.isMultiplication()) {
		for(size_t i = 0; i < m.size(); i++) {
			if(m[i].isPower() && m[i][1].representsNegative() && test_sign_zero(m[i][0], x_var, mzero, eo)) return true;
		}
	}
	CALCULATOR->beginTemporaryStopMessages();
	MathStructure mtest(m);
	mtest.replace(x_var, mzero);
	mtest.transform(COMPARISON_EQUALS, m_zero);
	mtest.eval(eo);
	return !CALCULATOR->endTemporaryStopMessages() && mtest.isOne();
}

bool try_sign(MathStructure &m1, MathStructure &m2, const MathStructure &marg, const MathStructure &x_var, const MathStructure &mzero, const EvaluationOptions &eo, bool *test_zero = NULL) {
	if(m1.equals(m2, true)) return true;
	if(m1.isNumber() && m1.number().isReal() && m2.isNumber() && m2.number().isReal()) {
		m2.number().negate();
		if(m1.number().equals(m2.number(), true)) {
			m2.number().negate();
			if(!test_sign_zero(m1, x_var, mzero, eo)) {
				if(!test_zero) return false;
				*test_zero = true;
			}
			if(!test_zero || !(*test_zero)) m1 *= MathStructure(CALCULATOR->f_signum, &marg, &m_zero, NULL);
			return true;
		}
		m2.number().negate();
		return false;
	}
	if(m1.type() != m2.type()) {
		if((m1.isMultiplication() && m1.size() == 2 && m1[0].isMinusOne() && try_sign(m1[1], m2, marg, x_var, mzero, eo)) || (m2.isMultiplication() && m2.size() == 2 && m2[0].isMinusOne() && try_sign(m1, m2[1], marg, x_var, mzero, eo))) {
			if(!test_sign_zero(m1, x_var, mzero, eo)) {
				if(!test_zero) return false;
				*test_zero = true;
			}
			if(!test_zero || !(*test_zero)) m1 *= MathStructure(CALCULATOR->f_signum, &marg, &m_zero, NULL);
			return true;
		}
		return false;
	}
	if(m1.size() == 0) return false;
	if(m1.size() != m2.size()) {
		if(m1.isMultiplication() && ((m1.size() == m2.size() + 1 && m1[0].isMinusOne()) || (m2.size() == m1.size() + 1 && m2[0].isMinusOne()))) {
			if(m1.size() > m2.size()) {
				for(size_t i = 0; i < m2.size(); i++) {
					if(!try_sign(m1[i + 1], m2[i], marg, x_var, mzero, eo)) return false;
				}
			} else {
				for(size_t i = 0; i < m1.size(); i++) {
					if(!try_sign(m1[i], m2[i + 1], marg, x_var, mzero, eo)) return false;
				}
			}
			if(!test_sign_zero(m1, x_var, mzero, eo)) {
				if(!test_zero) return false;
				*test_zero = true;
			}
			if(!test_zero || !(*test_zero)) m1 *= MathStructure(CALCULATOR->f_signum, &marg, &m_zero, NULL);
			return true;
		}
		return false;
	}
	if(m1.isFunction() && m1.function() != m2.function()) return false;
	if(m1.isComparison() && m1.comparisonType() != m2.comparisonType()) return false;
	if(m1.isMultiplication() && m1.size() > 1 && m1[0].isNumber() && !m1[0].equals(m2[0], true)) {
		if(!m1[0].isNumber()) return false;
		m2[0].number().negate();
		if(m1[0].number().equals(m2[0].number(), true)) {
			m2[0].number().negate();
			for(size_t i = 1; i < m1.size(); i++) {
				if(!try_sign(m1[i], m2[i], marg, x_var, mzero, eo)) return false;
			}
			if(!test_sign_zero(m1, x_var, mzero, eo)) {
				if(!test_zero) return false;
				*test_zero = true;
			}
			if(!test_zero || !(*test_zero)) m1 *= MathStructure(CALCULATOR->f_signum, &marg, &m_zero, NULL);
			return true;
		}
		m2[0].number().negate();
		return false;
	}/* else if(m1.isPower()) {
		bool b_tz = false;
		if(!try_sign(m1[0], m2[0], marg, x_var, mzero, eo, &b_tz) || b_tz) return false;
		if(!try_sign(m1[1], m2[1], marg, x_var, mzero, eo, &b_tz)) return false;
		if(b_tz && (test_sign_zero(m1[1], x_var, mzero, eo) || !test_sign_zero(m1[0], x_var, mzero, eo))) return false;
		return true;
	}*/
	bool b_tz = false;
	bool b_equal = false;
	for(size_t i = 0; i < m1.size(); i++) {
		if(!b_equal && m1.isAddition() && m1[i].equals(m2[i], true)) b_equal = true;
		else if(!try_sign(m1[i], m2[i], marg, x_var, mzero, eo, m1.isAddition() ? &b_tz : NULL)) return false;
		if(b_tz && b_equal) break;
	}
	if(b_tz) {
		if(!test_sign_zero(m1, x_var, mzero, eo)) {
			if(!test_zero) return false;
			*test_zero = true;
		}
		if(!test_zero || !(*test_zero)) m1 *= MathStructure(CALCULATOR->f_signum, &marg, &m_zero, NULL);
	}
	return true;
}

int MathStructure::integrate(const MathStructure &x_var, const EvaluationOptions &eo_pre, bool simplify_first, int use_abs, bool definite_integral, bool try_abs, int max_part_depth, vector<MathStructure*> *parent_parts) {
	if(CALCULATOR->aborted()) CANNOT_INTEGRATE
	EvaluationOptions eo = eo_pre;
	eo.protected_function = CALCULATOR->f_integrate;
	EvaluationOptions eo2 = eo;
	eo2.expand = true;
	eo2.combine_divisions = false;
	eo2.sync_units = false;
	EvaluationOptions eo_t = eo;
	eo_t.approximation = APPROXIMATION_TRY_EXACT;
	if(simplify_first) {
		unformat();
		calculateFunctions(eo2);
		calculatesub(eo2, eo2);
		if(CALCULATOR->aborted()) CANNOT_INTEGRATE
	}
	bool recalc = false;
	if(fix_abs_x(*this, x_var)) recalc = true;
	MathStructure *mfound = NULL;
	if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
		if(fix_sgn_x(*this, x_var, eo_t)) recalc = true;
		while(true) {
			mfound = find_abs_x(*this, x_var);
			if(mfound) {
				MathStructure m_interval(*mfound);
				m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
				CALCULATOR->beginTemporaryStopMessages();
				m_interval.eval(eo_t);
				if(!CALCULATOR->endTemporaryStopMessages()) break;
				if(m_interval.representsNonNegative(true)) {
					replace_abs_x(*this, MathStructure(*mfound), false);
					recalc = true;
				} else if(m_interval.representsNegative(true)) {
					replace_abs_x(*this, MathStructure(*mfound), true);
					recalc = true;
				} else {
					break;
				}
			} else {
				break;
			}
		}
	}
	if(recalc) calculatesub(eo2, eo2);
	if(equals(x_var)) {
		raise(2);
		multiply(MathStructure(1, 2, 0));
		return true;
	}
	if(containsRepresentativeOf(x_var, true, true) == 0) {
		multiply(x_var);
		return true;
	}
	mfound = find_abs_x(*this, x_var);
	if(try_abs && mfound) {
		MathStructure mtest(*this);
		MathStructure mtest_m(mtest);
		replace_abs_x(mtest, *mfound, false);
		replace_abs_x(mtest_m, *mfound, true);
		eo_t.isolate_x = true;
		eo_t.isolate_var = &x_var;
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mpos(*mfound);
		mpos.transform(COMPARISON_EQUALS_GREATER, m_zero);
		mpos.eval(eo_t);
		UnknownVariable *var_p = NULL, *var_m = NULL;
		if(!CALCULATOR->endTemporaryStopMessages() && mpos.isComparison() && (mpos.comparisonType() == COMPARISON_EQUALS_GREATER || mpos.comparisonType() == COMPARISON_EQUALS_LESS) && mpos[0] == x_var && mpos[1].isNumber()) {
			var_p = new UnknownVariable("", x_var.print());
			var_m = new UnknownVariable("", x_var.print());
			Number nr_interval_p, nr_interval_m;
			if(x_var.isVariable() && !x_var.variable()->isKnown() && ((UnknownVariable*) x_var.variable())->interval().isNumber() && ((UnknownVariable*) x_var.variable())->interval().number().isInterval() && ((UnknownVariable*) x_var.variable())->interval().number().isReal() && ((UnknownVariable*) x_var.variable())->interval().number().upperEndPoint().isGreaterThanOrEqualTo(mpos[1].number()) && ((UnknownVariable*) x_var.variable())->interval().number().lowerEndPoint().isLessThanOrEqualTo(mpos[1].number())) {
				if(mpos.comparisonType() == COMPARISON_EQUALS_GREATER) {
					nr_interval_p.setInterval(mpos[1].number(), ((UnknownVariable*) x_var.variable())->interval().number().upperEndPoint());
					nr_interval_m.setInterval(((UnknownVariable*) x_var.variable())->interval().number().lowerEndPoint(), mpos[1].number());
				} else {
					nr_interval_m.setInterval(mpos[1].number(), ((UnknownVariable*) x_var.variable())->interval().number().upperEndPoint());
					nr_interval_p.setInterval(((UnknownVariable*) x_var.variable())->interval().number().lowerEndPoint(), mpos[1].number());
				}
			} else {
				
				if(mpos.comparisonType() == COMPARISON_EQUALS_GREATER) {
					nr_interval_p.setInterval(mpos[1].number(), nr_plus_inf);
					nr_interval_m.setInterval(nr_minus_inf, mpos[1].number());
				} else {
					nr_interval_m.setInterval(mpos[1].number(), nr_plus_inf);
					nr_interval_p.setInterval(nr_minus_inf, mpos[1].number());
				}
			}
			var_p->setInterval(nr_interval_p);
			var_m->setInterval(nr_interval_m);
			mtest.replace(x_var, var_p);
			mtest_m.replace(x_var, var_m);
		}
		int bint1 = mtest.integrate(var_p ? var_p : x_var, eo, true, use_abs, true, definite_integral, max_part_depth, parent_parts);
		if(var_p) {
			mtest.replace(var_p, x_var);
			var_p->destroy();
		}
		if(bint1 <= 0 || mtest.containsFunction(CALCULATOR->f_integrate, true) > 0) {
			if(var_m) var_m->destroy();
			if(bint1 < 0) CANNOT_INTEGRATE_INTERVAL
			CANNOT_INTEGRATE;
		}
		int bint2 = mtest_m.integrate(var_m ? var_m : x_var, eo, true, use_abs, false, definite_integral, max_part_depth, parent_parts);
		if(var_m) {
			mtest_m.replace(var_m, x_var);
			var_m->destroy();
		}
		if(bint2 < 0) CANNOT_INTEGRATE_INTERVAL
		if(bint2 == 0 || mtest_m.containsFunction(CALCULATOR->f_integrate, true) > 0) CANNOT_INTEGRATE;
		MathStructure m1(mtest), m2(mtest_m);
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mzero(*mfound);
		mzero.transform(COMPARISON_EQUALS, m_zero);
		mzero.eval(eo_t);
		if(!CALCULATOR->endTemporaryStopMessages() && mzero.isComparison() && mzero.comparisonType() == COMPARISON_EQUALS && mzero[0] == x_var) {
			mzero.setToChild(2);
			CALCULATOR->beginTemporaryStopMessages();
			m1.calculatesub(eo2, eo2, true);
			m2.calculatesub(eo2, eo2, true);
			m1.evalSort(true, true);
			m2.evalSort(true, true);
			eo_t.test_comparisons = true;
			eo_t.isolate_x = false;
			eo_t.isolate_var = NULL;
			if(try_sign(m1, m2, *mfound, x_var, mzero, eo_t)) {
				set(m1, true);
				CALCULATOR->endTemporaryStopMessages(true);
				return true;
			}
			CALCULATOR->endTemporaryStopMessages();
		}
		MathStructure mcmp(*mfound);
		mcmp.transform(COMPARISON_EQUALS_GREATER, m_zero);
		set(mtest);
		multiply(mcmp);
		mcmp.setComparisonType(COMPARISON_LESS);
		mtest_m *= mcmp;
		add(mtest_m);
		return true;
	}

	switch(m_type) {
		case STRUCT_ADDITION: {
			bool b = false;
			MathStructure mbak(*this);
			for(size_t i = 0; i < SIZE; i++) {
				int bint = CHILD(i).integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts);
				if(bint < 0) {
					set(mbak);
					CANNOT_INTEGRATE_INTERVAL
				}
				if(bint > 0) b = true;
				CHILD_UPDATED(i);
			}
			if(!b) return false;
			break;
		}
		case STRUCT_UNIT: {}
		case STRUCT_NUMBER: {
			multiply(x_var);
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(1).isNumber() || CHILD(1).containsRepresentativeOf(x_var, true, true) == 0) {
				bool b_minusone = (CHILD(1).isNumber() && CHILD(1).number().isMinusOne());
				MathStructure madd, mmul, mmul2, mexp;
				if(integrate_info(CHILD(0), x_var, madd, mmul, mmul2, false, true)) {
					if(mmul2.isZero()) {
						if(madd.isZero() && mmul.isOne()) {
							if(b_minusone) {
								if(!transform_absln(CHILD(0), use_abs, definite_integral, x_var, eo)) CANNOT_INTEGRATE_INTERVAL
								SET_CHILD_MAP(0);
							} else {
								CHILD(1) += m_one;
								MathStructure mstruct(CHILD(1));
								divide(mstruct);
							}
							return true;
						} else if(b_minusone) {
							if(!transform_absln(CHILD(0), use_abs, definite_integral, x_var, eo)) CANNOT_INTEGRATE_INTERVAL
							SET_CHILD_MAP(0);
							if(!mmul.isOne()) divide(mmul);
							return true;
						} else {
							mexp = CHILD(1);
							mexp += m_one;
							SET_CHILD_MAP(0);
							raise(mexp);
							if(!mmul.isOne()) mexp *= mmul;
							divide(mexp);
							return true;
						}
					} else if(mmul.isZero() && !madd.isZero() && CHILD(1).number().denominatorIsTwo()) {
						MathStructure mmulsqrt2(mmul2);
						if(!mmul2.isOne()) mmulsqrt2 ^= nr_half;
						Number num(CHILD(1).number().numerator());
						if(num.isOne()) {
							MathStructure mthis(*this);
							add(x_var);
							if(!mmul2.isOne()) LAST *= mmulsqrt2;
							if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mthis); CANNOT_INTEGRATE_INTERVAL}
							multiply(madd);
							mthis *= x_var;
							if(!mmul2.isOne()) mthis *= mmulsqrt2;
							add(mthis);
							multiply(nr_half);
							if(!mmul2.isOne()) divide(mmulsqrt2);
							childrenUpdated(true);
							return true;
						} else if(num == 3) {
							MathStructure mterm3(*this);
							CHILD(1).number().set(1, 2, 0, true);
							MathStructure mterm2(*this);
							add(x_var);
							if(!mmul2.isOne()) LAST *= mmulsqrt2;
							if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mterm3); CANNOT_INTEGRATE_INTERVAL}
							multiply(madd);
							LAST ^= nr_two;
							multiply(Number(3, 8, 0), true);
							mterm2 *= x_var;
							if(!mmul2.isOne()) mterm2 *= mmulsqrt2;
							mterm2 *= madd;
							mterm2 *= Number(3, 8, 0);
							mterm3 *= x_var;
							if(!mmul2.isOne()) mterm3 *= mmulsqrt2;
							mterm3 *= Number(1, 4, 0);
							add(mterm2);
							add(mterm3);
							if(!mmul2.isOne()) divide(mmulsqrt2);
							return true;
						} else if(num == 5) {
							CHILD(1).number().set(1, 2, 0, true);
							MathStructure mterm2(*this);
							MathStructure mterm3(*this);
							MathStructure mterm4(*this);
							multiply(x_var);
							LAST ^= Number(5, 1);
							if(!mmul2.isOne()) {
								multiply(mmul2, true);
								LAST ^= nr_two;
							}
							multiply(Number(1, 6), true);
							if(!mmul2.isOne()) mterm2 *= mmulsqrt2;
							mterm2 += x_var;
							if(!mmul2.isOne()) mterm2[mterm2.size() - 1] *= mmul2;
							if(!transform_absln(mterm2, use_abs, definite_integral, x_var, eo)) {set(mterm3); CANNOT_INTEGRATE_INTERVAL}
							mterm2 *= madd;
							mterm2.last() ^= nr_three;
							if(!mmul2.isOne()) mterm2 /= mmulsqrt2;
							mterm2 *= Number(5, 16);
							mterm3 *= x_var;
							mterm3 *= madd;
							mterm3.last() ^= nr_two;
							mterm3 *= Number(11, 16);
							mterm4 *= x_var;
							mterm4.last() ^= nr_three;
							mterm4 *= madd;
							if(!mmul2.isOne()) mterm4 *= mmul2;
							mterm4 *= Number(13, 24);
							add(mterm2);
							add(mterm3);
							add(mterm4);
							childrenUpdated(true);
							return true;
						} else if(num.isMinusOne()) {
							MathStructure mbak(*this);
							CHILD(1).number().set(1, 2, 0, true);
							if(!mmul2.isOne()) multiply(mmulsqrt2);
							add(x_var);
							if(!mmul2.isOne()) LAST.multiply(mmul2);
							if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
							if(!mmul2.isOne()) divide(mmulsqrt2);
							return true;
						} else if(num == -3) {
							CHILD(1).number().set(-1, 2, 0, true);
							madd.inverse();
							multiply(madd);
							multiply(x_var);
							return true;
						}
					} else if(CHILD(1).isMinusOne()) {
						MathStructure m4acmb2(madd);
						m4acmb2 *= mmul2;
						m4acmb2 *= Number(4, 1);
						m4acmb2 += mmul;
						m4acmb2.last() ^= nr_two;
						m4acmb2.last().negate();
						m4acmb2.calculatesub(eo, eo, true);
						if(!m4acmb2.representsNonZero(true)) {
							if(m4acmb2.isZero()) {
								set(x_var, true);
								multiply(mmul2);
								multiply(nr_two, true);
								add(mmul);
								inverse();
								multiply(Number(-2, 1));
								return true;
							} else if(!warn_about_denominators_assumed_nonzero(m4acmb2, eo)) {
								CANNOT_INTEGRATE
							}
						}
						if(m4acmb2.representsNegative(true)) {
							MathStructure mbak(*this);
							MathStructure m2axpb(x_var);
							m2axpb *= mmul2;
							m2axpb *= nr_two;
							m2axpb += mmul;
							MathStructure mb2m4ac(madd);
							mb2m4ac *= mmul2;
							mb2m4ac *= Number(-4, 1);
							mb2m4ac += mmul;
							mb2m4ac.last() ^= nr_two;
							mb2m4ac ^= nr_half;
							set(m2axpb);
							subtract(mb2m4ac);
							multiply(m2axpb);
							LAST += mb2m4ac;
							LAST ^= nr_minus_one;
							if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
							divide(mb2m4ac);
							return true;
						}
						m4acmb2 ^= Number(-1, 2);
						set(x_var, true);
						multiply(mmul2);
						multiply(nr_two, true);
						add(mmul);
						multiply(m4acmb2);
						transform(CALCULATOR->f_atan);
						multiply(nr_two);
						multiply(m4acmb2, true);
						return true;
					} else if(CHILD(1).isInteger() && CHILD(1).number().isNegative()) {
						MathStructure m2nm3(CHILD(1));
						m2nm3 *= nr_two;
						m2nm3 += Number(3, 1);
						CHILD(1).number()++;
						MathStructure mnp1(CHILD(1));
						mnp1.number().negate();
						mnp1.number().recip();
						MathStructure mthis(*this);
						if(integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) CANNOT_INTEGRATE_INTERVAL
						MathStructure m4acmb2(madd);
						m4acmb2 *= mmul2;
						m4acmb2 *= Number(4, 1);
						m4acmb2 += mmul;
						m4acmb2.last() ^= nr_two;
						m4acmb2.last().negate();
						m4acmb2.inverse();
						MathStructure mfac1(mmul2);
						mfac1 *= Number(-2, 1);
						mfac1 *= m2nm3;
						mfac1 *= mnp1;
						mfac1 *= m4acmb2;
						multiply(mfac1);
						MathStructure mterm2(x_var);
						mterm2 *= mmul2;
						mterm2 *= nr_two;
						mterm2 += mmul;
						mterm2 *= m4acmb2;
						mterm2 *= mthis;
						mterm2 *= mnp1;
						add(mterm2);
						return true;
					}
				} else if(integrate_info(CHILD(0), x_var, madd, mmul, mexp, false, false) && mexp.isNumber() && !mexp.number().isOne()) {
					if(CHILD(1).isMinusOne() && mexp.number().isNegative() && mexp.number().isInteger()) {
						if(!madd.isZero()) {
							Number nexp(mexp.number());
							nexp.negate();
							MathStructure mtest(x_var);
							mtest ^= nexp;
							mtest *= madd;
							mtest += mmul;
							mtest.inverse();
							mtest *= x_var;
							mtest.last() ^= nexp;
							CALCULATOR->beginTemporaryStopMessages();
							if(mtest.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
								CALCULATOR->endTemporaryStopMessages(true);
								set(mtest, true);
								return true;
							}
							CALCULATOR->endTemporaryStopMessages();
						}
					} else if(mexp.number().isRational() && !mexp.number().isInteger()) {
						Number num(mexp.number().numerator());
						Number den(mexp.number().denominator());
						MathStructure morig(x_var);
						Number den_inv(den);
						den_inv.recip();
						morig ^= den_inv;
						UnknownVariable *var = new UnknownVariable("", string(LEFT_PARENTHESIS) + morig.print() + RIGHT_PARENTHESIS);
						Number den_m1(den);
						den_m1--;
						MathStructure mtest(var);
						if(!num.isOne()) mtest ^= num;
						if(!mmul.isOne()) mtest *= mmul;
						mtest += madd;
						mtest ^= CHILD(1);
						mtest *= var;
						if(!den_m1.isOne()) mtest.last() ^= den_m1;
						if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
							MathStructure m_interval(morig);
							m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
							var->setInterval(m_interval);
						} else {
							var->setInterval(morig);
						}
						CALCULATOR->beginTemporaryStopMessages();
						if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							set(mtest, true);
							replace(var, morig);
							multiply(den);
							var->destroy();
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
						var->destroy();
					}
				}
				if(CHILD(0).isFunction()) {
					MathStructure mbase(CHILD(0));
					int bint = integrate_function(mbase, x_var, eo, CHILD(1), m_one, use_abs, definite_integral, max_part_depth, parent_parts);
					if(bint < 0) CANNOT_INTEGRATE_INTERVAL
					if(bint) {
						set(mbase, true);
						return true;
					}
				}
				if(CHILD(0).isAddition()) {
					MathStructure mtest(*this);
					if(mtest[0].factorize(eo, false, 0, 0, false, false, NULL, x_var)) {
						mmul = m_one;
						while(mtest[0].isMultiplication() && mtest[0].size() >= 2 && mtest[0][0].containsRepresentativeOf(x_var, true, true) == 0) {
							if(mmul.isOne()) mmul = mtest[0][0];
							else mmul *= mtest[0][0];
							mtest[0].delChild(1, true);
						}
						if(!mmul.isOne()) {
							mmul ^= mtest[1];
						}
						MathStructure mtest2(mtest);
						if(mtest2.decomposeFractions(x_var, eo) && mtest2.isAddition()) {
							bool b = true;
							CALCULATOR->beginTemporaryStopMessages();
							for(size_t i = 0; i < mtest2.size(); i++) {
								if(mtest2[i].integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) <= 0) {
									b = false;
									break;
								}
							}
							CALCULATOR->endTemporaryStopMessages(b);
							if(b) {
								set(mtest2, true);
								if(!mmul.isOne()) multiply(mmul);
								return true;
							}
						}
						MathStructure mmul2(1, 1, 0);
						if(mtest[0].isMultiplication() && mtest[0].size() >= 2 && !mtest[0][0].isAddition()) {
							mmul2 = mtest[0][0];
							mmul2.calculateInverse(eo2);
							mtest[0].delChild(1, true);
						}
						if(mtest[0].isPower()) {
							mtest[1].calculateMultiply(mtest[0][1], eo2);
							mtest[0].setToChild(1);
						}
						if(!mmul2.isOne()) {
							mtest *= mmul2;
							mtest.evalSort(false);
						}
						CALCULATOR->beginTemporaryStopMessages();
						if(mtest.integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							set(mtest, true);
							if(!mmul.isOne()) multiply(mmul);
							return true;
						}
						CALCULATOR->endTemporaryStopMessages();
					}
					if(b_minusone) {
						MathStructure mmul2;
						if(integrate_info(CHILD(0), x_var, madd, mmul, mmul2, false, true) && (!mmul2.isZero() && (!madd.isZero() || !mmul.isZero()))) {
							if(mmul.isZero()) {
								if(!mmul2.isOne()) {
									madd /= mmul2;
								}
								madd ^= nr_half;
								set(x_var);
								divide(madd);
								transform(STRUCT_FUNCTION);
								setFunction(CALCULATOR->f_atan);
								divide(madd);
								if(!mmul2.isOne()) divide(mmul2);
								return true;
							} else {
								MathStructure acmb2(4, 1, 0);
								acmb2 *= mmul2;
								acmb2 *= madd;
								MathStructure b2(mmul);
								b2 ^= nr_two;
								acmb2 -= b2;
								acmb2 ^= nr_half;
								set(2, 1, 0, true);
								multiply(mmul2);
								multiply(x_var);
								add(mmul);
								divide(acmb2);
								transform(STRUCT_FUNCTION);
								setFunction(CALCULATOR->f_atan);
								multiply(nr_two);
								divide(acmb2);
								return true;
							}
						}
					}
				}
			} else if((CHILD(0).isNumber() && !CHILD(0).number().isOne()) || (!CHILD(0).isNumber() && CHILD(0).containsRepresentativeOf(x_var, true, true) == 0)) {
				MathStructure madd, mmul, mexp;
				if(integrate_info(CHILD(1), x_var, madd, mmul, mexp)) {
					if(mexp.isOne()) {
						if(CHILD(0) == CALCULATOR->v_e) {
							if(!mmul.isOne()) divide(mmul);
							return true;
						} else if(CHILD(0).isNumber() || warn_about_assumed_not_value(CHILD(0), m_one, eo)) {
							MathStructure mmulfac(CHILD(0));
							mmulfac.transform(CALCULATOR->f_ln);
							if(!mmul.isOne()) mmulfac *= mmul;
							divide(mmulfac);
							return true;
						}
					} else if(madd.isZero()) {
						if(mexp.number().isRational() && !mexp.isInteger()) {
							Number num(mexp.number().numerator());
							Number den(mexp.number().denominator());
							MathStructure morig(x_var);
							Number den_inv(den);
							den_inv.recip();
							morig ^= den_inv;
							UnknownVariable *var = new UnknownVariable("", string(LEFT_PARENTHESIS) + morig.print() + RIGHT_PARENTHESIS);
							MathStructure mvar(var);
							if(!num.isOne()) mvar ^= num;
							MathStructure mtest(CHILD(0));
							mtest ^= mvar;
							if(!mmul.isOne()) {
								mtest.last() *= mmul;
								mtest.childUpdated(mtest.size());
							}
							mtest *= mvar;
							if(num.isNegative()) mtest.last().last().number().negate();
							if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
								MathStructure m_interval(morig);
								m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
								var->setInterval(m_interval);
							} else {
								var->setInterval(morig);
							}
							CALCULATOR->beginTemporaryStopMessages();
							if(mtest.integrate(var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0 && mtest.containsFunction(CALCULATOR->f_integrate) <= 0) {
								CALCULATOR->endTemporaryStopMessages(true);
								set(mtest, true);
								replace(var, morig);
								multiply(den);
								var->destroy();
								return true;
							}
							CALCULATOR->endTemporaryStopMessages();
							var->destroy();
						}
						//integrate(a^(bx^c)) = -igamma(1/c, -b*ln(a)*x^c)/(c(-1)^(1/c)*b^(1/c)*ln(a)^(1/c))
						MathStructure mbase(CHILD(0));
						MathStructure mexpinv(mexp);
						mexpinv.inverse();
						MathStructure marg2(mbase);
						marg2.transform(CALCULATOR->f_ln);
						marg2 *= x_var;
						marg2.last() ^= mexp;
						marg2 *= mmul;
						marg2.negate();
						set(CALCULATOR->f_igamma, &mexpinv, &marg2, NULL);
						divide(mexp);
						mexpinv.negate();
						multiply(m_minus_one);
						LAST ^= mexpinv;
						multiply(mmul);
						LAST ^= mexpinv;
						multiply(mbase);
						LAST.transform(CALCULATOR->f_ln);
						LAST ^= mexpinv;
						negate();
						return true;
					}
				}
			}
			CANNOT_INTEGRATE
		}
		case STRUCT_FUNCTION: {
			MathStructure mfunc(*this);
			int bint = integrate_function(mfunc, x_var, eo, m_one, m_one, use_abs, definite_integral, max_part_depth, parent_parts);
			if(bint < 0) CANNOT_INTEGRATE_INTERVAL
			if(!bint) CANNOT_INTEGRATE
			set(mfunc, true);
			break;
		}
		case STRUCT_MULTIPLICATION: {
			MathStructure mstruct;
			bool b = false;
			for(size_t i = 0; i < SIZE;) {
				if(CHILD(i).containsRepresentativeOf(x_var, true, true) == 0) {
					if(b) {
						mstruct *= CHILD(i);
					} else {
						mstruct = CHILD(i);
						b = true;
					}
					ERASE(i);
				} else {
					i++;
				}
			}
			if(b) {
				if(SIZE == 1) {
					setToChild(1, true);
				} else if(SIZE == 0) {
					set(mstruct, true);
					break;	
				}
				if(integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) {
					multiply(mstruct);
					CANNOT_INTEGRATE_INTERVAL
				}
				multiply(mstruct);
				break;
			} else if(SIZE == 2) {
				if(CHILD(0) != x_var && (CHILD(1) == x_var || (CHILD(1).isPower() && CHILD(1)[0] == x_var))) SWAP_CHILDREN(0, 1);
				if(CHILD(1).isPower() && CHILD(1)[1].containsRepresentativeOf(x_var, true, true) == 0) {
					MathStructure madd, mmul, mmul2;
					MathStructure mexp(CHILD(1)[1]);
					if(integrate_info(CHILD(1)[0], x_var, madd, mmul, mmul2, false, true)) {
						if(mmul2.isZero() && mexp.isNumber()) {
							if(CHILD(0) == x_var) {
								MathStructure mbak(*this);
								SET_CHILD_MAP(1)
								SET_CHILD_MAP(0)
								if(mexp.number().isMinusOne()) {
									MathStructure mterm(x_var);
									if(!mmul.isOne()) mterm /= mmul;
									if(!madd.isZero()) {
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										multiply(madd);
										if(!mmul.isOne()) {
											MathStructure a2(mmul);
											a2 ^= nr_two;
											divide(a2);
										}
										negate();
										add(mterm);
									} else {
										set(mterm, true);
									}
								} else if(mexp.number() == -2) {
									MathStructure mterm(*this);
									if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
									MathStructure a2(mmul);
									if(!mmul.isOne()) {
										a2 ^= nr_two;
										divide(a2);
									}
									if(!madd.isZero()) {
										if(!mmul.isOne()) mterm *= a2;
										mterm.inverse();
										mterm *= madd;
									}
									add(mterm);
								} else if(mexp.number().isNegative()) {
									MathStructure onemn(1, 1, 0);
									onemn += mexp;
									MathStructure nm1(mexp);
									nm1.negate();
									MathStructure nm2(nm1);
									nm1 += nr_minus_one;
									nm2 += Number(-2, 1);
									raise(nm1);
									multiply(nm2);
									multiply(nm1);
									if(!mmul.isOne()) {
										MathStructure a2(mmul);
										a2 ^= nr_two;
										multiply(a2);
									}
									inverse();
									MathStructure mnum(x_var);
									mnum *= onemn;
									if(!mmul.isOne()) mnum *= mmul;
									if(!madd.isZero()) mnum -= madd;
									multiply(mnum);
								} else {
									MathStructure nm1(mexp);
									nm1 += m_one;
									raise(nm1);
									MathStructure mnum(x_var);
									mnum *= nm1;
									if(!mmul.isOne()) mnum *= mmul;
									mnum -= madd;
									MathStructure mden(mexp);
									mden += nr_two;
									mden *= nm1;
									if(!mmul.isOne()) {
										mden *= mmul;
										mden.last() ^= nr_two;
									}
									multiply(mnum);
									divide(mden);
									return true;
								}
								return true;
							} else if(CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1] == nr_two) {
								MathStructure mbak(*this);
								SET_CHILD_MAP(1)
								SET_CHILD_MAP(0)
								if(mexp.number().isMinusOne()) {
									MathStructure mterm(x_var);
									mterm ^= nr_two;
									if(!mmul.isOne()) mterm *= mmul;
									if(!madd.isZero()) {
										MathStructure mtwobx(-2 ,1, 0);
										mtwobx *= madd;
										mtwobx *= x_var;
										mterm += mtwobx;
									}
									if(!mmul.isOne()) {
										MathStructure a2(mmul);
										a2 ^= nr_two;
										a2 *= nr_two;
										mterm /= a2;
									} else {
										mterm /= nr_two;
									}
									if(!madd.isZero()) {
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										MathStructure b2(madd);
										b2 ^= nr_two;
										multiply(b2);
										if(!mmul.isOne()) {
											MathStructure a3(mmul);
											a3 ^= nr_three;
											divide(a3);
										}
										add(mterm);
									} else {
										set(mterm, true);
									}
								} else if(mexp.number() == -2) {
									MathStructure mterm1(x_var);
									if(!mmul.isOne()) mterm1 *= mmul;
									if(!madd.isZero()) {
										MathStructure mterm2(*this);
										if(!transform_absln(mterm2, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										mterm2 *= madd;
										mterm2 *= -2;
										MathStructure mterm3(*this);
										mterm3.inverse();
										madd ^= nr_two;
										mterm3 *= madd;
										mterm3.negate();
										set(mterm1);
										add(mterm2);
										add(mterm3);
									} else {
										set(mterm1);
									}
									if(!mmul.isOne()) {
										MathStructure a3(mmul);
										a3 ^= nr_three;
										divide(a3);
									}
								} else if(mexp.number() == -3) {
									if(!madd.isZero()) {
										MathStructure mterm2(*this);
										mterm2.inverse();
										mterm2 *= madd;
										mterm2 *= nr_two;
										MathStructure mterm3(*this);
										mterm3 ^= nr_two;
										mterm3 *= nr_two;
										mterm3.inverse();
										madd ^= nr_two;
										mterm3 *= madd;
										mterm3.negate();
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										add(mterm2);
										add(mterm3);
									} else {
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
									}
									if(!mmul.isOne()) {
										MathStructure a3(mmul);
										a3 ^= nr_three;
										divide(a3);
									}
								} else {
									MathStructure mterm2(*this);
									MathStructure mterm3(*this);
									raise(nr_three);
									CHILD(1) += mexp;
									MathStructure mden(mexp);
									mden += nr_three;
									divide(mden);
									if(!madd.isZero()) {
										mterm2 ^= nr_two;
										mterm2[1] += mexp;
										mterm2 *= madd;
										mterm2 *= -2;
										mden = mexp;
										mden += nr_two;
										mterm2 /= mden;
										mterm3 ^= m_one;
										mterm3[1] += mexp;
										madd ^= nr_two;
										mterm3 *= madd;
										mden = mexp;
										mden += m_one;
										mterm3 /= mden;
										add(mterm2);
										add(mterm3);
									}
									if(!mmul.isOne()) {
										MathStructure a3(mmul);
										a3 ^= nr_three;
										divide(a3);
									}
								}
								return true;
							} else if(CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1] == nr_minus_one && !madd.isZero()) {
								if(mexp.number().isMinusOne()) {
									MathStructure mbak(*this);
									SET_CHILD_MAP(1)
									SET_CHILD_MAP(0)
									divide(x_var);
									if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
									divide(madd);
									negate();
									return true;
								}
							} else if(CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1] == -2 && !madd.isZero()) {
								if(mexp.number().isMinusOne()) {
									MathStructure mbak(*this);
									SET_CHILD_MAP(1)
									SET_CHILD_MAP(0)
									divide(x_var);
									if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
									MathStructure madd2(madd);
									madd2 ^= nr_two;
									divide(madd2);
									if(!mmul.isOne()) multiply(mmul);
									madd *= x_var;
									madd.inverse();
									subtract(madd);
									return true;
								} else if(mexp.number() == -2) {
									MathStructure mbak(*this);
									SET_CHILD_MAP(1)
									SET_CHILD_MAP(0)
									MathStructure mterm2(madd);
									mterm2 ^= nr_two;
									MathStructure mterm3(mterm2);
									mterm2 *= *this;
									mterm2.inverse();
									divide(x_var);
									if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
									MathStructure madd3(madd);
									madd3 ^= nr_three;
									divide(madd3);
									multiply(Number(-2, 1, 0));
									mterm3 *= x_var;
									if(!mmul.isOne()) mterm3 *= mmul;
									mterm3.inverse();
									add(mterm2);
									add(mterm3);
									mmul.negate();
									multiply(mmul);
									return true;
								}
							}
						} else if(mmul.isZero() && !madd.isZero() && mexp.isNumber() && mexp.number().denominatorIsTwo()) {
							Number num(mexp.number());
							if(CHILD(0) == x_var) {
								num.add(1);
								SET_CHILD_MAP(1)
								SET_CHILD_MAP(0)
								raise(num);
								num.multiply(2);
								num.recip();
								multiply(num);
								if(!mmul2.isOne()) divide(mmul2);
								return true;
							} else if(CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isInteger()) {
								if(CHILD(0)[1].number() == 2) {
									if(num == nr_half) {
										MathStructure mbak(*this);
										SET_CHILD_MAP(1)
										MathStructure mterm2(*this);
										if(!mmul2.isOne()) {
											mterm2 *= mmul2;
											mterm2.last() ^= nr_half;
										}
										mterm2 += x_var;
										if(!mmul2.isOne()) mterm2.last() *= mmul2;
										if(!transform_absln(mterm2, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										mterm2.multiply(madd);
										mterm2.last() ^= nr_two;
										multiply(x_var);
										if(!mmul2.isOne()) {
											multiply(mmul2);
											LAST ^= nr_half;
										}
										multiply(x_var);
										LAST ^= nr_two;
										if(!mmul2.isOne()) LAST *= mmul2;
										LAST *= nr_two;
										LAST += madd;
										subtract(mterm2);
										multiply(Number(1, 8));
										if(!mmul2.isOne()) {
											multiply(mmul2);
											LAST ^= Number(-3, 2);
										}
										return true;
									}
								} else if(CHILD(0)[1].number() == -1) {
									if(num == nr_minus_half) {
										MathStructure mbak(*this);
										SET_CHILD_MAP(1)
										SET_CHILD_MAP(0)
										raise(nr_half);
										multiply(madd);
										LAST ^= nr_half;
										add(madd);
										transform(CALCULATOR->f_ln);
										negate();
										add(x_var);
										if(!transform_absln(LAST, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										multiply(madd);
										LAST ^= nr_minus_half;
										return true;
									} else if(num == nr_half) {
										MathStructure mbak(*this);
										SET_CHILD_MAP(1)
										MathStructure mterm2(*this);
										mterm2 *= madd;
										mterm2.last() ^= nr_half;
										mterm2 += madd;
										mterm2.transform(CALCULATOR->f_ln);
										mterm2 *= madd;
										mterm2.last() ^= nr_half;
										MathStructure mterm3(x_var);
										if(!transform_absln(mterm3, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										mterm3 *= madd;
										mterm3.last() ^= nr_half;
										subtract(mterm2);
										add(mterm3);
										return true;
									}
								}
							}
						} else if(mmul2.isZero()) {
							if(CHILD(0) == x_var) {
								SET_CHILD_MAP(1)
								CHILD(1) += m_one;
								MathStructure mnum(x_var);
								mnum *= CHILD(1);
								if(!mmul.isOne()) mnum *= mmul;
								mnum -= madd;
								MathStructure mden(CHILD(1)[0]);
								mden += nr_two;
								mden *= CHILD(1);
								if(!mmul.isOne()) { 
									mden *= mmul;
									mden[mden.size() - 1] ^= nr_two;
								}
								multiply(mnum);
								divide(mden);
								return true;
							}
						} else if(mexp.isMinusOne() && CHILD(0) == x_var) {
							MathStructure mbak(*this);
							SET_CHILD_MAP(1)
							MathStructure mterm2(CHILD(0));
							if(integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) CANNOT_INTEGRATE_INTERVAL
							multiply(mmul);
							divide(mmul2);
							multiply(Number(-1, 2));
							if(!transform_absln(mterm2, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
							mterm2 /= mmul2;
							mterm2 *= nr_half;
							add(mterm2);
							return true;
						} else if(mexp.isMinusOne() && CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isMinusOne()) {
							MathStructure mbak(*this);
							SET_CHILD_MAP(1)
							MathStructure mterm2(CHILD(0));
							if(integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) CANNOT_INTEGRATE_INTERVAL
							multiply(mmul);
							divide(madd);
							multiply(Number(-1, 2));
							mterm2.inverse();
							mterm2 *= x_var;
							mterm2.last() ^= nr_two;
							if(!transform_absln(mterm2, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
							mterm2 /= madd;
							mterm2 *= nr_half;
							add(mterm2);
							return true;
						} else if(mexp.isInteger() && mexp.number().isNegative() && CHILD(0) == x_var) {
							MathStructure mbak(*this);
							SET_CHILD_MAP(1)
							MathStructure m2nm3(CHILD(1));
							m2nm3 *= nr_two;
							m2nm3 += Number(3, 1);
							CHILD(1).number()++;
							MathStructure mnp1(CHILD(1));
							mnp1.number().negate();
							mnp1.number().recip();
							MathStructure mthis(*this);
							if(integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
							MathStructure m4acmb2(madd);
							m4acmb2 *= mmul2;
							m4acmb2 *= Number(4, 1);
							m4acmb2 += mmul;
							m4acmb2.last() ^= nr_two;
							m4acmb2.last().negate();
							m4acmb2.inverse();
							MathStructure mfac1(mmul);
							mfac1 *= m2nm3;
							mfac1 *= mnp1;
							mfac1 *= m4acmb2;
							multiply(mfac1);
							MathStructure mterm2(x_var);
							mterm2 *= mmul;
							mterm2 += madd;
							mterm2.last() *= nr_two;
							mterm2 *= m4acmb2;
							mterm2 *= mthis;
							mterm2 *= mnp1;
							subtract(mterm2);
							return true;
						}
					}
					if(CHILD(0) == x_var || (CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isRational())) {
						MathStructure madd, mmul, mpow;
						if(integrate_info(CHILD(1)[0], x_var, madd, mmul, mpow, false, false) && mpow.isNumber() && mpow.number().isRational() & !mpow.number().isOne()) {
							Number nexp(1, 1, 0);
							if(CHILD(0).isPower()) nexp = CHILD(0)[1].number();
							if(!nexp.isOne() && mpow.isInteger()) {
								if(mexp.isMinusOne() && nexp.isMinusOne()) {
									MathStructure mbak(*this);
									if(mpow.number().isNegative()) {
										set(x_var, true);
										mpow.number().negate();
										raise(mpow);
										multiply(madd);
										if(!mmul.isOne()) add(mmul);
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										divide(mpow);
										if(!mmul.isOne()) divide(mmul);
									} else {
										SET_CHILD_MAP(1)
										SET_CHILD_MAP(0)
										if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										add(x_var);
										if(!transform_absln(LAST, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
										LAST *= mpow;
										LAST.negate();
										negate();
										divide(mpow);
										divide(madd);
									}
									return true;
								}
								Number mpowmexp(mpow.number());
								mpowmexp -= nexp;
								if(mpowmexp.isOne()) {
									if(mexp.isMinusOne()) {
										MathStructure mbak(*this);
										if(mpow.number().isNegative()) {
											set(x_var, true);
											mpow.number().negate();
											raise(mpow);
											multiply(madd);
											if(!mmul.isOne()) add(mmul);
											if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
											divide(mpow);
											divide(madd);
											add(x_var);
											if(!transform_absln(LAST, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
											LAST.negate();
											divide(mpow);
											divide(madd);
											negate();
										} else {
											SET_CHILD_MAP(1)
											SET_CHILD_MAP(0)
											if(!transform_absln(*this, use_abs, definite_integral, x_var, eo)) {set(mbak); CANNOT_INTEGRATE_INTERVAL}
											divide(mpow);
											if(!mmul.isOne()) divide(mmul);
										}
									} else {
										SET_CHILD_MAP(1)
										MathStructure mden(CHILD(1));
										CHILD(1) += m_one;
										mden *= mpow;
										if(!mmul.isOne()) mden *= mmul;
										mden += mpow;
										if(!mmul.isOne()) mden.last() *= mmul;
										divide(mden);
									}
									return true;
								}
							}
							for(int i = 1; i <= 3; i++) {
								if(i > 1 && (!mpow.isInteger() || !mpow.number().isIntegerDivisible(i) || mpow.number() == i)) break;
								UnknownVariable *var = NULL;
								MathStructure m_replace(x_var);
								MathStructure mtest(CHILD(1));
								Number new_pow(nexp);
								b = false;
								if(i == 1) {
									m_replace ^= mpow;
									var = new UnknownVariable("", string(LEFT_PARENTHESIS) + m_replace.print() + RIGHT_PARENTHESIS);
									mtest.replace(m_replace, var);
									new_pow++;
									new_pow -= mpow.number();
									new_pow /= mpow.number();
									b = true;
								} else if(i == 2) {
									new_pow = nexp;
									new_pow++;
									new_pow -= 2;
									new_pow /= 2;
									if(new_pow.isInteger()) {
										b = true;
										m_replace ^= nr_two;
										var = new UnknownVariable("", string(LEFT_PARENTHESIS) + m_replace.print() + RIGHT_PARENTHESIS);
										MathStructure m_prev(x_var), m_new(var);
										m_prev ^= mpow;
										m_new ^= mpow;
										m_new[1].number() /= 2;
										mtest.replace(m_prev, m_new);
									}
								} else if(i == 3) {
									new_pow++;
									new_pow -= 3;
									new_pow /= 3;
									if(new_pow.isInteger()) {
										b = true;
										m_replace ^= nr_three;
										var = new UnknownVariable("", string(LEFT_PARENTHESIS) + m_replace.print() + RIGHT_PARENTHESIS);
										MathStructure m_prev(x_var), m_new(var);
										m_prev ^= mpow;
										m_new ^= mpow;
										m_new[1].number() /= 3;
										mtest.replace(m_prev, m_new);
									}
								}
								if(b) {
									if(!new_pow.isZero()) {
										mtest *= var;
										mtest.swapChildren(1, mtest.size());
										if(!new_pow.isOne()) mtest[0] ^= new_pow;
									}
									CALCULATOR->beginTemporaryStopMessages();
									if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
										MathStructure m_interval(m_replace);
										m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
										var->setInterval(m_interval);
									} else {
										var->setInterval(m_replace);
									}
									if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
										CALCULATOR->endTemporaryStopMessages(true);
										mtest.replace(var, m_replace);
										set(mtest, true);
										divide(m_replace[1]);
										var->destroy();
										return true;
									}
									CALCULATOR->endTemporaryStopMessages();
									var->destroy();
								}
							}
						}
					}
					if(CHILD(1)[1].number().isMinusOne() && CHILD(1)[0].isMultiplication() && CHILD(1)[0].size() == 2 && CHILD(1)[0][0].isAddition() && CHILD(1)[0][1].isAddition() && (CHILD(0) == x_var || (CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isTwo()))) {
						MathStructure madd1, mmul1, mexp1;
						if(integrate_info(CHILD(1)[0][0], x_var, madd1, mmul1, mexp1) && mexp1.isOne()) {
							MathStructure madd2, mexp2;
							if(integrate_info(CHILD(1)[0][1], x_var, madd2, mmul2, mexp2) && mexp2.isOne()) {
								MathStructure mnum1, mnum2, mx1(madd2), mx2(madd1);
								mx1.negate();
								if(!mmul2.isOne()) mx1 /= mmul2;
								mnum1 = mx1;
								if(!mmul1.isOne()) mx1 *= mmul1;
								mx1 += madd1;
								mnum1 /= mx1;
								mx2.negate();
								if(!mmul1.isOne()) mx2 /= mmul1;
								mnum2 = mx2;
								if(!mmul2.isOne()) mx2 *= mmul2;
								mx2 += madd2;
								mnum2 /= mx2;
								mnum2 /= CHILD(1)[0][0];
								mnum1 /= CHILD(1)[0][1];
								if(CHILD(0) != x_var) {
									mnum1 *= x_var;
									mnum2 *= x_var;
								}
								mnum2 += mnum1;
								CALCULATOR->beginTemporaryStopMessages();
								if(mnum2.integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
									CALCULATOR->endTemporaryStopMessages(true);
									set(mnum2, true);
									return true;
								}
								CALCULATOR->endTemporaryStopMessages();
							}
						}
					}
					if(CHILD(1)[0].isAddition()) {
						bool b_poly = false;
						if(CHILD(1)[1].isMinusOne()) {
							MathStructure mquo, mrem;
							b_poly = polynomial_long_division(CHILD(0), CHILD(1)[0], x_var, mquo, mrem, eo, true);
							if(b_poly && !mquo.isZero()) {
								MathStructure mtest(mquo);
								if(!mrem.isZero()) {
									mtest += mrem;
									mtest.last() *= CHILD(1);
									mtest.childrenUpdated();
								}
								CALCULATOR->beginTemporaryStopMessages();
								if(mtest.integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
									CALCULATOR->endTemporaryStopMessages(true);
									set(mtest, true);
									return true;
								}
								CALCULATOR->endTemporaryStopMessages();
							}
						}
						MathStructure mtest(*this);
						if(mtest[1][0].factorize(eo, false, 0, 0, false, false, NULL, x_var)) {
							mmul = m_one;
							while(mtest[1][0].isMultiplication() && mtest[1][0].size() >= 2 && mtest[1][0][0].containsRepresentativeOf(x_var, true, true) == 0) {
								if(mmul.isOne()) mmul = mtest[1][0][0];
								else mmul *= mtest[1][0][0];
								mtest[1][0].delChild(1, true);
							}
							if(!mmul.isOne()) {
								mmul ^= CHILD(1)[1];
							}
							if(b_poly) {
								MathStructure mtest2(mtest);
								if(mtest2.decomposeFractions(x_var, eo)) {
									if(mtest2.isAddition()) {
										bool b = true;
										CALCULATOR->beginTemporaryStopMessages();
										for(size_t i = 0; i < mtest2.size(); i++) {
											if(mtest2[i].integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) <= 0) {
												b = false;
												break;
											}
										}
										CALCULATOR->endTemporaryStopMessages(b);
										if(b) {
											set(mtest2, true);
											if(!mmul.isOne()) multiply(mmul);
											return true;
										}
									}
								} else {
									mtest2 = mtest[1];
									if(mtest2.decomposeFractions(x_var, eo) && mtest2.isAddition()) {
										if(mtest2.isAddition()) {
											bool b = true;
											CALCULATOR->beginTemporaryStopMessages();
											for(size_t i = 0; i < mtest2.size(); i++) {
												mtest2[i] *= mtest[0];
												if(mtest2[i].integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) <= 0) {
													b = false;
													break;
												}
											}
											CALCULATOR->endTemporaryStopMessages(b);
											if(b) {
												set(mtest2, true);
												if(!mmul.isOne()) multiply(mmul);
												return true;
											}
										}
									}
								}
							}
							mmul2 = m_one;
							if(mtest[1][0].isMultiplication() && mtest[1][0].size() >= 2 && !mtest[1][0][0].isAddition()) {
								mmul2 = mtest[1][0][0];
								mmul2.calculateInverse(eo2);
								mtest[1][0].delChild(1, true);
							}
							if(mtest[1][0].isPower()) {
								mtest[1][1].calculateMultiply(mtest[1][0][1], eo2);
								mtest[1][0].setToChild(1);
							}
							if(!mmul2.isOne()) {
								mtest *= mmul2;
								mtest.evalSort(false);
							}
							CALCULATOR->beginTemporaryStopMessages();
							if(mtest.integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
								CALCULATOR->endTemporaryStopMessages(true);
								set(mtest);
								if(!mmul.isOne()) multiply(mmul);
								return true;
							}
							CALCULATOR->endTemporaryStopMessages();
						}
						if(CHILD(1)[1].number().isMinusOne() && CHILD(0) == x_var) {
							if(integrate_info(CHILD(1)[0], x_var, madd, mmul, mmul2, false, true) && !mmul2.isZero() && mmul.isZero() && !madd.isZero()) {
								SET_CHILD_MAP(1)
								SET_CHILD_MAP(0)
								transform(STRUCT_FUNCTION);
								setFunction(CALCULATOR->f_ln);
								mmul2 *= Number(-2, 1);
								multiply(mmul2);
								return true;
							}
						}
					}
				} else if(CHILD(1).isPower() && ((CHILD(1)[0].isNumber() && !CHILD(1)[0].number().isOne()) || (!CHILD(1)[0].isNumber() && CHILD(1)[0].containsRepresentativeOf(x_var, true, true) == 0))) {
					MathStructure mexp(1, 1, 0);
					if(CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isInteger()) mexp = CHILD(0)[1];
					else if(CHILD(0) != x_var) CANNOT_INTEGRATE;
					MathStructure madd, mmul, mpow;
					if(integrate_info(CHILD(1)[1], x_var, madd, mmul, mpow, false, false) && mpow.isInteger()) {
						bool b_e = CHILD(1)[0] == CALCULATOR->v_e;
						if(b_e || CHILD(1)[0].isNumber() || warn_about_assumed_not_value(CHILD(1)[0], m_one, eo)) {
							if(mpow.isOne()) {
								SET_CHILD_MAP(1)
								if(!b_e) {
									if(mmul.isOne()) {
										mmul = CHILD(0);
										mmul.transform(CALCULATOR->f_ln);
									} else {
										MathStructure lnbase(CALCULATOR->f_ln, &CHILD(0), NULL);
										mmul *= lnbase;
									}
								}
								if(mexp.isOne()) {
									MathStructure mmulfac(x_var);
									if(!mmul.isOne()) mmulfac *= mmul;
									mmulfac += nr_minus_one;
									multiply(mmulfac);
									if(!mmul.isOne()) {
										mmul ^= Number(-2, 1);;
										multiply(mmul);
									}
								} else if(mexp.number().isTwo()) {
									MathStructure mmulfac(x_var);
									mmulfac ^= nr_two;
									if(!mmul.isOne()) mmulfac /= mmul;
									mmulfac += x_var;
									mmulfac.last() *= Number(-2, 1);
									if(!mmul.isOne()) {
										mmulfac.last() *= mmul;
										mmulfac.last().last() ^= Number(-2, 1);
									}
									mmulfac += nr_two;
									if(!mmul.isOne()) {
										mmulfac.last() *= mmul;
										mmulfac.last().last() ^= Number(-3, 1);
									}
									mmulfac.childrenUpdated(true);
									multiply(mmulfac);
								} else if(mexp.isMinusOne()) {
									set(x_var, true);
									if(!mmul.isOne()) multiply(mmul);
									transform(CALCULATOR->f_Ei);
								} else if(mexp.number().isNegative()) {
									MathStructure mterm2(*this);
									mexp += m_one;
									multiply(x_var);
									LAST ^= mexp;
									CHILDREN_UPDATED
									if(integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) CANNOT_INTEGRATE_INTERVAL
									if(!mmul.isOne()) multiply(mmul);
									mterm2 *= x_var;
									mterm2.last() ^= mexp;
									mterm2.childrenUpdated();
									subtract(mterm2);
									mexp.negate();
									divide(mexp);
								} else {
									MathStructure mterm2(*this);
									multiply(x_var);
									LAST ^= mexp;
									LAST[1] += nr_minus_one;
									LAST.childUpdated(2);
									CHILDREN_UPDATED
									if(integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts) < 0) CANNOT_INTEGRATE_INTERVAL
									multiply(mexp);
									if(!mmul.isOne()) divide(mmul);
									negate();
									mterm2 *= x_var;
									mterm2.last() ^= mexp;
									mterm2.childrenUpdated();
									if(!mmul.isOne()) mterm2.divide(mmul);
									add(mterm2);
								}
								return true;
							} else {
								Number mpowmexp(mpow.number());
								mpowmexp -= mexp.number();
								if(mpowmexp.isOne()) {
									SET_CHILD_MAP(1)
									MathStructure malog(CALCULATOR->f_ln, &CHILD(0), NULL);
									divide(mpow);
									if(!mmul.isOne()) divide(mmul);
									if(!b_e) divide(malog);
									return true;
								} else if(mexp.isMinusOne() && mpow.number().isPositive()) {
									MathStructure malog;
									if(b_e) {
										malog = x_var;
										malog ^= mpow;
									} else {
										malog.set(CALCULATOR->f_ln, &CHILD(1)[0], NULL);
										malog *= x_var;
										malog.last() ^= mpow;
									}
									if(!mmul.isOne()) malog *= mmul;
									malog.transform(CALCULATOR->f_Ei);
									if(madd.isZero()) {
										set(malog, true);
									} else {
										SET_CHILD_MAP(1)
										SET_CHILD_MAP(0)
										raise(madd);
										multiply(malog);
									}
									divide(mpow);
									return true;
								}
							}
						}
					}
				}
			}
			if(SIZE >= 2) {
				for(size_t i = 0; i < SIZE; i++) {
					if((CHILD(i).isFunction() && CHILD(i).function() == CALCULATOR->f_signum) || (CHILD(i).isPower() && CHILD(i)[0].isFunction() && CHILD(i)[0].function() == CALCULATOR->f_signum)) {
						MathStructure mfunc(CHILD(i).isPower() ? CHILD(i)[0] : CHILD(i));
						MathStructure mmul(*this);
						mmul.delChild(i + 1, true);
						CALCULATOR->beginTemporaryStopMessages();
						int bint = integrate_function(mfunc, x_var, eo, CHILD(i).isPower() ? CHILD(i)[1] : m_one, mmul, use_abs, definite_integral, max_part_depth, parent_parts);
						CALCULATOR->endTemporaryStopMessages(bint > 0);
						if(bint < 0) CANNOT_INTEGRATE_INTERVAL
						if(bint) {
							set(mfunc, true);
							return true;
						}
					}
				}
				for(size_t i = 0; i < SIZE; i++) {
					if((CHILD(i).isFunction() && CHILD(i).function() != CALCULATOR->f_signum) || (CHILD(i).isPower() && CHILD(i)[0].isFunction() && CHILD(i)[0].function() != CALCULATOR->f_signum)) {
						MathStructure mfunc(CHILD(i).isPower() ? CHILD(i)[0] : CHILD(i));
						MathStructure mmul(*this);
						mmul.delChild(i + 1, true);
						CALCULATOR->beginTemporaryStopMessages();
						int bint = integrate_function(mfunc, x_var, eo, CHILD(i).isPower() ? CHILD(i)[1] : m_one, mmul, use_abs, definite_integral, max_part_depth, parent_parts);
						CALCULATOR->endTemporaryStopMessages(bint > 0);
						if(bint < 0) CANNOT_INTEGRATE_INTERVAL
						if(bint) {
							set(mfunc, true);
							return true;
						}
					}
				}
				if(CHILD(0) == x_var || (CHILD(0).isPower() && CHILD(0)[0] == x_var && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isRational())) {
					Number nexp(1, 1, 0);
					if(CHILD(0).isPower()) nexp = CHILD(0)[1].number();
					MathStructure mpow(1, 1, 0);
					bool b = true;
					MathStructure madd, mmul, mpown;
					for(size_t i = 1; i < SIZE; i++) {
						if((CHILD(i).isFunction() && CHILD(i).size() == 1) || (SIZE > 2 && CHILD(i).isPower() && CHILD(i)[1].containsRepresentativeOf(x_var, true, true) == 0)) {
							if(!integrate_info(CHILD(i)[0], x_var, madd, mmul, mpown, false, false) || !mpown.isNumber() || !mpown.number().isRational() || mpown.number().isOne() || (!mpow.isOne() && mpow != mpown)) {
								b = false;
								break;
							}
							mpow = mpown;
						} else if(CHILD(i).isPower() && CHILD(i)[0].containsRepresentativeOf(x_var, true, true) == 0) {
							if(!integrate_info(CHILD(i)[1], x_var, madd, mmul, mpown, false, false) || !mpown.isNumber() || !mpown.number().isRational() || mpown.number().isOne() || (!mpow.isOne() && mpow != mpown)) {
								b = false;
								break;
							}
							mpow = mpown;
						} else if(CHILD(i).isPower() && CHILD(i)[1].containsRepresentativeOf(x_var, true, true) == 0) {
							if(!integrate_info(CHILD(i)[0], x_var, madd, mmul, mpown, false, false) || !mpown.isNumber() || !mpown.number().isRational() || mpown.number().isInteger() || mpown.number().isOne() || (!mpow.isOne() && mpow != mpown)) {
								b = false;
								break;
							}
							mpow = mpown;
						} else {
							b = false;
							break;
						}

					}
					if(b) {
						for(int i = 1; i <= 3; i++) {
							if(!mpow.isInteger()) {
								if(i > 2) break;
							} else if(i > 1 && (!mpow.number().isIntegerDivisible(i) || mpow.number() == i)) {
								break;
							}
							if(CALCULATOR->aborted()) CANNOT_INTEGRATE
							UnknownVariable *var = NULL;
							MathStructure m_replace(x_var);
							MathStructure mtest(*this);
							mtest.delChild(1, true);
							Number new_pow(nexp);
							b = false;
							if(i == 1) {
								m_replace ^= mpow;
								var = new UnknownVariable("", "");
								mtest.replace(m_replace, var);
								new_pow++;
								new_pow -= mpow.number();
								new_pow /= mpow.number();
								b = true;
							} else if(i == 2) {
								if(!mpow.number().isInteger()) {
									Number nden = mpow.number().denominator();
									nden.recip();
									nden--;
									nden.negate();
									new_pow = nexp;
									new_pow += nden;
									new_pow *= mpow.number().denominator();
									if(new_pow.isInteger()) {
										b = true;
										m_replace ^= mpow.number().denominator();
										m_replace[1].number().recip();
										var = new UnknownVariable("", "");
										MathStructure m_prev(x_var), m_new(var);
										m_prev ^= mpow;
										if(!mpow.number().numeratorIsOne()) m_new ^= mpow.number().numerator();
										mtest.replace(m_prev, m_new);
									}
								} else {
									new_pow = nexp;
									new_pow++;
									new_pow -= 2;
									new_pow /= 2;
									if(new_pow.isInteger()) {
										b = true;
										m_replace ^= nr_two;
										var = new UnknownVariable("", "");
										MathStructure m_prev(x_var), m_new(var);
										m_prev ^= mpow;
										m_new ^= mpow;
										m_new[1].number() /= 2;
										mtest.replace(m_prev, m_new);
									}
								}
							} else if(i == 3) {
								new_pow++;
								new_pow -= 3;
								new_pow /= 3;
								if(new_pow.isInteger()) {
									b = true;
									m_replace ^= nr_three;
									var = new UnknownVariable("", "");
									MathStructure m_prev(x_var), m_new(var);
									m_prev ^= mpow;
									m_new ^= mpow;
									m_new[1].number() /= 3;
									mtest.replace(m_prev, m_new);
								}
							}
							if(b) {
								if(!new_pow.isZero()) {
									mtest *= var;
									mtest.swapChildren(1, mtest.size());
									if(!new_pow.isOne()) mtest[0] ^= new_pow;
								}
								CALCULATOR->beginTemporaryStopMessages();
								var->setName(string(LEFT_PARENTHESIS) + m_replace.print() + RIGHT_PARENTHESIS);
								if(x_var.isVariable() && !x_var.variable()->isKnown() && !((UnknownVariable*) x_var.variable())->interval().isUndefined()) {
									MathStructure m_interval(m_replace);
									m_interval.replace(x_var, ((UnknownVariable*) x_var.variable())->interval());
									var->setInterval(m_interval);
								} else {
									var->setInterval(m_replace);
								}
								if(mtest.integrate(var, eo, false, use_abs, definite_integral, true, max_part_depth, parent_parts) > 0) {
									CALCULATOR->endTemporaryStopMessages(true);
									mtest.replace(var, m_replace);
									set(mtest, true);
									if(m_replace.isPower()) divide(m_replace[1]);
									var->destroy();
									return true;
								}
								CALCULATOR->endTemporaryStopMessages();
								var->destroy();
							}
						}
					}
				}
				vector<MathStructure*> parent_parts_pre;
				if(parent_parts) {
					for(size_t i = 0; i < parent_parts->size(); i++) {
						if(equals(*(*parent_parts)[i], true)) CANNOT_INTEGRATE
					}
				} else {
					parent_parts = &parent_parts_pre;
				}
				size_t pp_size = parent_parts->size();
				bool b = false;
				for(size_t i = 0; !b && max_part_depth > 0 && (i < SIZE || (SIZE == 3 && i < SIZE * 2)) && SIZE < 10; i++) {
					if(CALCULATOR->aborted()) CANNOT_INTEGRATE
					CALCULATOR->beginTemporaryStopMessages();
					// integration by parts
					MathStructure mstruct_u;
					MathStructure mstruct_v;
					MathStructure minteg_v;
					if(SIZE == 3 && i >= 3) {
						mstruct_v = CHILD(i - 3);
						mstruct_u = *this; 
						mstruct_u.delChild(i - 2);
					} else {
						mstruct_u = CHILD(i);
						if(SIZE == 2 && i == 0) mstruct_v = CHILD(1);
						else if(SIZE == 2 && i == 1) mstruct_v = CHILD(0);
						else {mstruct_v = *this; mstruct_v.delChild(i + 1);}
					}
					MathStructure mdiff_u(mstruct_u);
					if(mdiff_u.differentiate(x_var, eo2) && mdiff_u.containsFunction(CALCULATOR->f_diff, true) <= 0) {
						mdiff_u.calculateFunctions(eo2);
						minteg_v = mstruct_v;
						if(minteg_v.integrate(x_var, eo, false, use_abs, definite_integral, true, 0, parent_parts) > 0) {
							parent_parts->push_back(this);
							MathStructure minteg_2(minteg_v);
							if(!mdiff_u.isOne()) minteg_2 *= mdiff_u;
							minteg_2.calculateFunctions(eo2);
							minteg_2.calculatesub(eo2, eo2, true);
							if(minteg_2.countTotalChildren() < 100 && minteg_2.integrate(x_var, eo, false, use_abs, definite_integral, true, max_part_depth - 1, parent_parts) > 0) {
								int cui = contains_unsolved_integrate(minteg_2, this, parent_parts);
								if(cui == 3) {
									MathStructure mfunc(CALCULATOR->f_integrate, this, &x_var, &m_undefined, &m_undefined, NULL);
									UnknownVariable *var = new UnknownVariable("", mfunc.print());
									var->setAssumptions(mfunc);
									MathStructure mvar(var);
									minteg_2.replace(mfunc, mvar);
									MathStructure msolve(mstruct_u);
									msolve.multiply(minteg_v);
									msolve.calculatesub(eo2, eo2, true);
									msolve.subtract(minteg_2);
									msolve.calculatesub(eo2, eo2, true);
									MathStructure msolve_d(msolve);
									if(msolve_d.differentiate(mvar, eo2) && COMPARISON_IS_NOT_EQUAL(msolve_d.compareApproximately(m_one, eo2))) {
										msolve.transform(COMPARISON_EQUALS, mvar);
										msolve.isolate_x(eo2, mvar);
										if(msolve.isComparison() && msolve.comparisonType() == COMPARISON_EQUALS && msolve[0] == mvar && msolve[1].contains(mvar, true) <= 0) {
											b = true;
											set(msolve[1], true);
										}
									}
								} else if(cui != 1) {
									set(mstruct_u);
									multiply(minteg_v);
									subtract(minteg_2);
									b = true;
								}
							}
							parent_parts->pop_back();
						}
					}
					CALCULATOR->endTemporaryStopMessages(b);
				}
				while(parent_parts->size() > pp_size) parent_parts->pop_back();
				if(b) return true;
			}
			CANNOT_INTEGRATE
			break;
		}
		case STRUCT_SYMBOLIC: {
			if(representsNumber(true)) {
				multiply(x_var);
			} else {
				CANNOT_INTEGRATE
			}
			break;
		}
		case STRUCT_VARIABLE: {
			if(eo.calculate_variables && o_variable->isKnown()) {
				if(eo.approximation != APPROXIMATION_EXACT || !o_variable->isApproximate()) {
					set(((KnownVariable*) o_variable)->get(), true);
					return integrate(x_var, eo, true, use_abs, definite_integral, true, max_part_depth, parent_parts);
				} else if(containsRepresentativeOf(x_var, true, true) != 0) {
					CANNOT_INTEGRATE
				}
			}
			if(representsNumber(true)) {
				multiply(x_var);
				break;
			}
		}		
		default: {
			CANNOT_INTEGRATE
		}	
	}
	return true;
}


const MathStructure &MathStructure::find_x_var() const {
	if(isSymbolic()) {
		return *this;
	} else if(isVariable()) {
		if(o_variable->isKnown()) return m_undefined;
		return *this;
	}
	const MathStructure *mstruct;
	const MathStructure *x_mstruct = &m_undefined;
	for(size_t i = 0; i < SIZE; i++) {
		mstruct = &CHILD(i).find_x_var();
		if(mstruct->isVariable()) {
			if(mstruct->variable() == CALCULATOR->v_x) {
				return *mstruct;
			} else if(!x_mstruct->isVariable()) {
				x_mstruct = mstruct;
			} else if(mstruct->variable() == CALCULATOR->v_y) {
				x_mstruct = mstruct;
			} else if(mstruct->variable() == CALCULATOR->v_z && x_mstruct->variable() != CALCULATOR->v_y) {
				x_mstruct = mstruct;
			}
		} else if(mstruct->isSymbolic()) {
			if(!x_mstruct->isVariable() && !x_mstruct->isSymbolic()) {
				x_mstruct = mstruct;
			}
		}
	}
	return *x_mstruct;
}

bool isUnit_multi(const MathStructure &mstruct) {
	if(!mstruct.isMultiplication() || mstruct.size() == 0) return false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if((i > 0 || !mstruct[i].isNumber()) && !mstruct[i].isUnit_exp()) return false;
	}
	return true;
}

int test_comparisons(const MathStructure &msave, MathStructure &mthis, const MathStructure &x_var, const EvaluationOptions &eo, bool sub) {
	if(mthis.isComparison() && mthis[0] == x_var) {
		MathStructure mtest;
		EvaluationOptions eo2 = eo;
		eo2.calculate_functions = false;
		eo2.isolate_x = false;
		eo2.test_comparisons = true;
		eo2.warn_about_denominators_assumed_nonzero = false;
		eo2.assume_denominators_nonzero = false;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mtest = mthis;
		mtest.eval(eo2);
		if(mtest.isComparison()) {
			mtest = msave;
			mtest.replace(x_var, mthis[1]);
			if(CALCULATOR->usesIntervalArithmetic()) {
				MathStructure mtest2(mtest[0]);
				if(!mtest[1].isZero()) {
					mtest2.subtract(mtest[1]);
				}
				CALCULATOR->beginTemporaryStopMessages();
				mtest2.eval(eo2);
				if(CALCULATOR->endTemporaryStopMessages() > 0) {
					if(!sub) mthis = msave;
					return -1;
				}
				if(mtest2.isNumber()) {
					if(!mtest2.number().isNonZero()) return 1;
					else if(mtest2.number().isInterval() || mtest2.number().isRational()) {mthis.clear(); return 0;}
				} else if(mtest2.isUnit_exp()) {
					mthis.clear(); return 0;
				} else if(isUnit_multi(mtest2)) {
					if(!mtest2[0].isNumber()) {mthis.clear(); return 0;}
					if(!mtest2[0].number().isNonZero()) return 1;
					else if(mtest2[0].number().isInterval() || mtest2[0].number().isRational()) {mthis.clear(); return 0;}
				}
				
			}
			CALCULATOR->beginTemporaryStopMessages();
			if(mtest[1].isZero() && mtest[0].isAddition() && mtest[0].size() > 1) {
				mtest[1].subtract(mtest[0][0]);
				mtest[0].delChild(1, true);
			}
			mtest.eval(eo2);
			if(CALCULATOR->endTemporaryStopMessages() > 0) {
				if(!sub) mthis = msave;
				return -1;
			}
			if(mtest.isNumber()) {
				if(mtest.number().getBoolean() == 1) {
					return 1;
				} else if(mtest.number().getBoolean() == 0) {
					mthis.clear();
					return 0;
				}
			}
		} else {
			mthis = mtest;
			if(mtest.isNumber()) {
				if(mtest.number().getBoolean() == 0) {
					return 0;
				}
			}
			return 1;
		}
		if(!sub) mthis = msave;
		return -1;
	}
	if(mthis.isLogicalOr() || mthis.isLogicalAnd()) {
		for(size_t i = 0; i < mthis.size(); i++) {
			if(test_comparisons(msave, mthis[i], x_var, eo, true) < 0) {
				mthis = msave;
				return -1;
			}
		}
		return 1;
	}
	if(sub) return 1;
	else return -1;
}

bool isx_deabsify(MathStructure &mstruct);
bool isx_deabsify(MathStructure &mstruct) {
	switch(mstruct.type()) {
		case STRUCT_FUNCTION: {
			if(mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1 && mstruct[0].representsNonComplex(true)) {
				mstruct.setToChild(1, true);
				return true;
			}
			break;
		}
		case STRUCT_POWER: {
			if(mstruct[1].isMinusOne()) {
				return isx_deabsify(mstruct[0]);
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(isx_deabsify(mstruct[i])) b = true;
			}
			return b;
		}
		default: {}
	}
	return false;
}

int newton_raphson(const MathStructure &mstruct, MathStructure &x_value, const MathStructure &x_var, const EvaluationOptions &eo) {
	if(mstruct == x_var) {
		x_value = m_zero;
		return 1;
	}

	if(!mstruct.isAddition()) return -1;
	if(mstruct.size() == 2) {
		if(mstruct[1] == x_var && mstruct[0].isNumber() && mstruct[0].number().isReal()) {
			x_value = mstruct[0];
			x_value.number().negate();
			return 1;
		}
		if(mstruct[0] == x_var && mstruct[1].isNumber() && mstruct[1].number().isReal()) {
			x_value = mstruct[1];
			x_value.number().negate();
			return 1;
		}
	}

	Number nr;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(mstruct[i] != x_var) {
			switch(mstruct[i].type()) {
				case STRUCT_NUMBER: {nr = mstruct[i].number(); break;}
				case STRUCT_MULTIPLICATION: {
					if(mstruct[i].size() == 2 && mstruct[i][0].isNumber() && (mstruct[i][1] == x_var || (mstruct[i][1].isPower() && mstruct[i][1][0] == x_var && mstruct[i][1][1].isNumber() && mstruct[i][1][1].number().isInteger() && mstruct[i][1][1].number().isPositive()))) {
						break;
					}
					return -1;
				}
				case STRUCT_POWER: {
					if(mstruct[i][0] == x_var && mstruct[i][1].isNumber() && mstruct[i][1].number().isInteger() && mstruct[i][1].number().isPositive()) {
						break;
					}
				}
				default: {return -1;}
			}
		}
	}

	MathStructure mdiff(mstruct);
	if(!mdiff.differentiate(x_var, eo)) return -1;
	MathStructure minit(mstruct);
	minit.divide(mdiff);
	minit.negate();
	minit.add(x_var);
	minit.eval(eo);

	Number nr_target_high(1, 1, -(PRECISION) - 10);
	Number nr_target_low(1, 1, 0);
	nr_target_low -= nr_target_high;
	nr_target_high++;
	MathStructure mguess(2, 1, 0);
	Number ndeg(mstruct.degree(x_var));
	bool overflow = false;
	int ideg = ndeg.intValue(&overflow);
	if(overflow || ideg > 100) return -1;
	if(!nr.isZero()) {
		if(nr.isNegative()) nr.negate();
		if(nr.root(ndeg)) mguess = nr;
	}
	PrintOptions po;
	po.interval_display = INTERVAL_DISPLAY_INTERVAL;
	
	for(int i = 0; i < 100 + PRECISION + ideg * 2; i++) {

		if(CALCULATOR->aborted()) return -1;

		MathStructure mtest(minit);
		mtest.replace(x_var, mguess);
		mtest.eval(eo);

		Number nrdiv(mguess.number());
		if(!mtest.isNumber() || !nrdiv.divide(mtest.number())) {
			return -1;
		}

		if(nrdiv.isLessThan(nr_target_high) && nrdiv.isGreaterThan(nr_target_low)) {
			if(CALCULATOR->usesIntervalArithmetic()) {
				if(!x_value.number().setInterval(mguess.number(), mtest.number())) return -1;
			} else {
				x_value = mtest;
				if(x_value.number().precision() < 0 || x_value.number().precision() > PRECISION + 10) x_value.number().setPrecision(PRECISION + 10);
			}
			x_value.numberUpdated();
			return 1;
		}
		mguess = mtest;
	}
	return 0;
}

int find_interval_precision(const MathStructure &mstruct);

int find_interval_precision(const MathStructure &mstruct) {
	if(mstruct.isNumber()) {
		return mstruct.number().precision(1);
	}
	int iv_prec = -1;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(iv_prec > -1) {
		 	if(find_interval_precision(mstruct[i]) > -1) return 0;
		} else {
			iv_prec = find_interval_precision(mstruct[i]);
		}
	}
	return iv_prec;
}

MathStructure *find_abs_sgn(MathStructure &mstruct, const MathStructure &x_var);
MathStructure *find_abs_sgn(MathStructure &mstruct, const MathStructure &x_var) {
	switch(mstruct.type()) {
		case STRUCT_FUNCTION: {
			if(((mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1) || (mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2)) && mstruct[0].contains(x_var, false) && mstruct[0].representsNonComplex()) {
				return &mstruct;
			}
			break;
		}
		case STRUCT_POWER: {
			return find_abs_sgn(mstruct[0], x_var);
		}
		case STRUCT_ADDITION: {}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < mstruct.size(); i++) {
				MathStructure *m = find_abs_sgn(mstruct[i], x_var);
				if(m) return m;
			}
			break;
		}
		default: {break;}
	}
	return NULL;
}

bool solve_x_pow_x(Number &nr) {
	// x^x=a => x=ln(a)/lambertw(ln(a))
	Number n_ln_z(nr);
	if(n_ln_z.ln()) {
		Number n_ln_z_w(n_ln_z);
		if(n_ln_z_w.lambertW()) {
			if(n_ln_z.divide(n_ln_z_w)) {
				nr = n_ln_z;
				return true;
			}
		}
	}
	return false;
}

bool is_units_with_multiplier(const MathStructure &mstruct) {
	if(!mstruct.isMultiplication() || mstruct.size() == 0 || !mstruct[0].isNumber()) return false;
	for(size_t i = 1; i < mstruct.size(); i++) {
		if(!mstruct[i].isUnit_exp()) return false;
	}
	return true;
}

bool MathStructure::isolate_x_sub(const EvaluationOptions &eo, EvaluationOptions &eo2, const MathStructure &x_var, MathStructure *morig) {
	if(!isComparison()) {
		printf("isolate_x_sub: not comparison\n");
		return false;
	}
	if(CHILD(0) == x_var) return false;
	if(contains(x_var, true) <= 0) return false;
	
	if(CALCULATOR->aborted()) return false;

	switch(CHILD(0).type()) {
		case STRUCT_ADDITION: {
			bool b = false;
			// x+y=z => x=z-y
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(!CHILD(0)[i].contains(x_var)) {
					CHILD(0)[i].calculateNegate(eo2);
					CHILD(0)[i].ref();
					CHILD(1).add_nocopy(&CHILD(0)[i], true);
					CHILD(1).calculateAddLast(eo2);
					CHILD(0).delChild(i + 1);
					b = true;
				}
			}
			if(b) {
				CHILD_UPDATED(0);
				CHILD_UPDATED(1);
				if(CHILD(0).size() == 1) {
					CHILD(0).setToChild(1, true);
				} else if(CHILD(0).size() == 0) {
					CHILD(0).clear(true);
				}
				isolate_x_sub(eo, eo2, x_var, morig);
				return true;
			}
			if(CALCULATOR->aborted()) return false;

			// ax^2+bx=c
			if(CHILD(0).size() >= 2 && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
				bool sqpow = false, nopow = false;
				MathStructure mstruct_a, mstruct_b;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CHILD(0)[i].isPower() && CHILD(0)[i][0] == x_var && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number() == 2) {
						if(sqpow) mstruct_a.add(m_one, true);
						else {sqpow = true; mstruct_a.set(m_one);}
					} else if(CHILD(0)[i] == x_var) {
						if(nopow) mstruct_b.add(m_one, true);
						else {nopow = true; mstruct_b.set(m_one);}
					} else if(CHILD(0)[i].isMultiplication()) {
						for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
							if(CHILD(0)[i][i2] == x_var) {
								if(nopow) {
									MathStructure *madd = new MathStructure(CHILD(0)[i]);
									madd->delChild(i2 + 1, true);
									mstruct_b.add_nocopy(madd, true);
								} else {
									nopow = true; 
									mstruct_b = CHILD(0)[i];
									mstruct_b.delChild(i2 + 1, true);
								}
							} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][0] == x_var && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number() == 2) {
								if(sqpow) {
									MathStructure *madd = new MathStructure(CHILD(0)[i]);
									madd->delChild(i2 + 1, true);
									mstruct_a.add_nocopy(madd, true);
								} else {
									sqpow = true;
									mstruct_a = CHILD(0)[i];
									mstruct_a.delChild(i2 + 1, true);
								}
							} else if(CHILD(0)[i][i2].contains(x_var)) {
								sqpow = false;
								nopow = false;
								break;
							}
						}
						if(!sqpow && !nopow) break;
					} else {
						sqpow = false;
						nopow = false;
						break;
					}
				}
				bool b = false;
				if(sqpow && nopow && !mstruct_a.representsZero(true)) {
					b = mstruct_a.representsNonZero(true);
					if(!b && eo2.approximation == APPROXIMATION_EXACT) {
						MathStructure mtest(mstruct_a);
						mtest.add(m_zero, OPERATION_NOT_EQUALS);
						EvaluationOptions eo3 = eo2;
						eo3.test_comparisons = true;
						mtest.calculatesub(eo3, eo, false);
						b = mtest.isOne();
					}
				}
				if(b) {
					int a_iv = find_interval_precision(mstruct_a);
					int b_iv = find_interval_precision(mstruct_b);
					int c_iv = find_interval_precision(CHILD(1));
					if(a_iv >= 0 && (c_iv < 0 || c_iv > a_iv) && CHILD(1).representsNonZero()) {
						//x=(-2c)/(b+/-sqrt(b^2-4ac))
						MathStructure mbak(*this);
						bool stop_iv = false;
						if(c_iv >= 0 && c_iv <= PRECISION) {
							stop_iv = true;
						} else if(b_iv >= 0) {
							MathStructure mstruct_bl;
							MathStructure mstruct_bu;
							stop_iv = true;
							if(mstruct_b.isNumber() && mstruct_b.number().isNonZero() && !mstruct_b.number().hasImaginaryPart()) {
								mstruct_bl = mstruct_b.number().lowerEndPoint();
								mstruct_bu = mstruct_b.number().upperEndPoint();
								stop_iv = false;
							} else if(is_units_with_multiplier(mstruct_b) && mstruct_b[0].number().isNonZero() && !mstruct_b[0].number().hasImaginaryPart()) {
								mstruct_bl = mstruct_b;
								mstruct_bl[0].number() = mstruct_b[0].number().lowerEndPoint();
								mstruct_bu = mstruct_b;
								mstruct_bu[0].number() = mstruct_b[0].number().upperEndPoint();
								stop_iv = false;
							}
							if(!stop_iv) {
								// Lower b+sqrt(b^2-4ac)
								MathStructure b2l(mstruct_bl);
								b2l.calculateRaise(nr_two, eo2);
								MathStructure ac(4, 1);
								ac.calculateMultiply(mstruct_a, eo2);
								ac.calculateMultiply(CHILD(1), eo2);
								b2l.calculateAdd(ac, eo2);
									
								b2l.calculateRaise(nr_half, eo2);
								MathStructure mstruct_1l(mstruct_bl);
								mstruct_1l.calculateAdd(b2l, eo2);
								
								// Upper -b+sqrt(b^2-4ac)
								MathStructure b2u(mstruct_bu);
								b2u.calculateRaise(nr_two, eo2);
								b2u.calculateAdd(ac, eo2);
									
								b2u.calculateRaise(nr_half, eo2);
								MathStructure mstruct_1u(mstruct_bu);
								mstruct_1u.calculateAdd(b2u, eo2);
								
								MathStructure mstruct_1(mstruct_1l);
								mstruct_1.transform(STRUCT_FUNCTION, mstruct_1u);
								mstruct_1.setFunction(CALCULATOR->f_interval);
								mstruct_1.calculateFunctions(eo2, false);
								
								// Lower -b-sqrt(b^2-4ac)
								MathStructure mstruct_2l(mstruct_bl);
								mstruct_2l.calculateSubtract(b2l, eo2);
								
								// Upper -b-sqrt(b^2-4ac)
								MathStructure mstruct_2u(mstruct_bu);
								mstruct_2u.calculateSubtract(b2u, eo2);
								
								MathStructure mstruct_2(mstruct_2l);
								mstruct_2.transform(STRUCT_FUNCTION, mstruct_2u);
								mstruct_2.setFunction(CALCULATOR->f_interval);
								mstruct_2.calculateFunctions(eo2, false);
								
								MathStructure mstruct_c(CHILD(1));
								mstruct_c.calculateMultiply(nr_two, eo2);
								
								mstruct_1.calculateInverse(eo2);
								mstruct_2.calculateInverse(eo2);
								mstruct_1.calculateMultiply(mstruct_c, eo2);
								mstruct_2.calculateMultiply(mstruct_c, eo2);
								
								CHILD(0) = x_var;
								if(mstruct_1 == mstruct_2) {
									CHILD(1) = mstruct_1;
								} else {
									CHILD(1) = mstruct_1;
									MathStructure *mchild2 = new MathStructure(CHILD(0));
									mchild2->transform(STRUCT_COMPARISON, mstruct_2);
									mchild2->setComparisonType(ct_comp);
									if(ct_comp == COMPARISON_NOT_EQUALS) {
										transform_nocopy(STRUCT_LOGICAL_AND, mchild2);
									} else {
										transform_nocopy(STRUCT_LOGICAL_OR, mchild2);
									}
									calculatesub(eo2, eo, false);
								}
								CHILDREN_UPDATED
								return true;
							}
						}
						if(stop_iv) {
							CALCULATOR->beginTemporaryStopIntervalArithmetic();
							bool failed = false;
							fix_intervals(*this, eo2, &failed);
							if(failed) {
								set(mbak);
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								b = false;
							}
						}
						if(b) {
							MathStructure b2(mstruct_b);
							b2.calculateRaise(nr_two, eo2);
							MathStructure ac(4, 1);
							ac.calculateMultiply(mstruct_a, eo2);
							ac.calculateMultiply(CHILD(1), eo2);
							b2.calculateAdd(ac, eo2);
								
							b2.calculateRaise(nr_half, eo2);
							MathStructure mstruct_1(mstruct_b);
							mstruct_1.calculateAdd(b2, eo2);
							MathStructure mstruct_2(mstruct_b);
							mstruct_2.calculateSubtract(b2, eo2);
							
							MathStructure mstruct_c(CHILD(1));
							mstruct_c.calculateMultiply(nr_two, eo2);
							
							mstruct_1.calculateInverse(eo2);
							mstruct_2.calculateInverse(eo2);
							mstruct_1.calculateMultiply(mstruct_c, eo2);
							mstruct_2.calculateMultiply(mstruct_c, eo2);
							
							CHILD(0) = x_var;
							if(mstruct_1 == mstruct_2) {
								CHILD(1) = mstruct_1;
							} else {
								CHILD(1) = mstruct_1;
								MathStructure *mchild2 = new MathStructure(CHILD(0));
								mchild2->transform(STRUCT_COMPARISON, mstruct_2);
								mchild2->setComparisonType(ct_comp);
								if(ct_comp == COMPARISON_NOT_EQUALS) {
									transform_nocopy(STRUCT_LOGICAL_AND, mchild2);
								} else {
									transform_nocopy(STRUCT_LOGICAL_OR, mchild2);
								}
								calculatesub(eo2, eo, false);
							}
							CHILDREN_UPDATED;
							if(stop_iv) {
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
								fix_intervals(*this, eo2);
							}
							return true;
						}
					} else {
						//x=(-b+/-sqrt(b^2-4ac))/(2a)
						MathStructure mbak(*this);
						bool stop_iv = false;
						if(a_iv >= 0 && a_iv <= PRECISION) {
							stop_iv = true;
						} else if(b_iv >= 0) {
							MathStructure mstruct_bl;
							MathStructure mstruct_bu;
							stop_iv = true;
							if(mstruct_b.isNumber() && mstruct_b.number().isNonZero() && !mstruct_b.number().hasImaginaryPart()) {
								mstruct_bl = mstruct_b.number().lowerEndPoint();
								mstruct_bu = mstruct_b.number().upperEndPoint();
								stop_iv = false;
							} else if(is_units_with_multiplier(mstruct_b) && mstruct_b[0].number().isNonZero() && !mstruct_b[0].number().hasImaginaryPart()) {
								mstruct_bl = mstruct_b;
								mstruct_bl[0].number() = mstruct_b[0].number().lowerEndPoint();
								mstruct_bu = mstruct_b;
								mstruct_bu[0].number() = mstruct_b[0].number().upperEndPoint();
								stop_iv = false;
							}
							if(!stop_iv) {
								// Lower -b+sqrt(b^2-4ac)
								MathStructure b2l(mstruct_bl);
								b2l.calculateRaise(nr_two, eo2);
								MathStructure ac(4, 1);
								ac.calculateMultiply(mstruct_a, eo2);
								ac.calculateMultiply(CHILD(1), eo2);
								b2l.calculateAdd(ac, eo2);
									
								b2l.calculateRaise(nr_half, eo2);
								mstruct_bl.calculateNegate(eo2);
								MathStructure mstruct_1l(mstruct_bl);
								mstruct_1l.calculateAdd(b2l, eo2);
								
								// Upper -b+sqrt(b^2-4ac)
								MathStructure b2u(mstruct_bu);
								b2u.calculateRaise(nr_two, eo2);
								b2u.calculateAdd(ac, eo2);
									
								b2u.calculateRaise(nr_half, eo2);
								mstruct_bu.calculateNegate(eo2);
								MathStructure mstruct_1u(mstruct_bu);
								mstruct_1u.calculateAdd(b2u, eo2);
								
								MathStructure mstruct_1(mstruct_1l);
								mstruct_1.transform(STRUCT_FUNCTION, mstruct_1u);
								mstruct_1.setFunction(CALCULATOR->f_interval);
								mstruct_1.calculateFunctions(eo2, false);
								
								// Lower -b-sqrt(b^2-4ac)
								MathStructure mstruct_2l(mstruct_bl);
								mstruct_2l.calculateSubtract(b2l, eo2);
								
								// Upper -b-sqrt(b^2-4ac)
								MathStructure mstruct_2u(mstruct_bu);
								mstruct_2u.calculateSubtract(b2u, eo2);
								
								MathStructure mstruct_2(mstruct_2l);
								mstruct_2.transform(STRUCT_FUNCTION, mstruct_2u);
								mstruct_2.setFunction(CALCULATOR->f_interval);
								mstruct_2.calculateFunctions(eo2, false);
								
								mstruct_a.calculateMultiply(nr_two, eo2);
								mstruct_a.calculateInverse(eo2);
								mstruct_1.calculateMultiply(mstruct_a, eo2);
								mstruct_2.calculateMultiply(mstruct_a, eo2);
								CHILD(0) = x_var;
								if(mstruct_1 == mstruct_2) {
									CHILD(1) = mstruct_1;
								} else {
									CHILD(1) = mstruct_1;
									MathStructure *mchild2 = new MathStructure(CHILD(0));
									mchild2->transform(STRUCT_COMPARISON, mstruct_2);
									mchild2->setComparisonType(ct_comp);
									if(ct_comp == COMPARISON_NOT_EQUALS) {
										transform_nocopy(STRUCT_LOGICAL_AND, mchild2);
									} else {
										transform_nocopy(STRUCT_LOGICAL_OR, mchild2);
									}
									calculatesub(eo2, eo, false);
								}
								CHILDREN_UPDATED
								return true;
							}
						}
						if(stop_iv) {
							CALCULATOR->beginTemporaryStopIntervalArithmetic();
							bool failed = false;
							fix_intervals(*this, eo2, &failed);
							if(failed) {
								set(mbak);
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								b = false;
							}
						}
						if(b) {
							MathStructure b2(mstruct_b);
							b2.calculateRaise(nr_two, eo2);
							MathStructure ac(4, 1);
							ac.calculateMultiply(mstruct_a, eo2);
							ac.calculateMultiply(CHILD(1), eo2);
							b2.calculateAdd(ac, eo2);
								
							b2.calculateRaise(nr_half, eo2);
							mstruct_b.calculateNegate(eo2);
							MathStructure mstruct_1(mstruct_b);
							mstruct_1.calculateAdd(b2, eo2);
							MathStructure mstruct_2(mstruct_b);
							mstruct_2.calculateSubtract(b2, eo2);
							mstruct_a.calculateMultiply(nr_two, eo2);
							mstruct_a.calculateInverse(eo2);
							mstruct_1.calculateMultiply(mstruct_a, eo2);
							mstruct_2.calculateMultiply(mstruct_a, eo2);
							CHILD(0) = x_var;
							if(mstruct_1 == mstruct_2) {
								CHILD(1) = mstruct_1;
							} else {
								CHILD(1) = mstruct_1;
								MathStructure *mchild2 = new MathStructure(CHILD(0));
								mchild2->transform(STRUCT_COMPARISON, mstruct_2);
								mchild2->setComparisonType(ct_comp);
								if(ct_comp == COMPARISON_NOT_EQUALS) {
									transform_nocopy(STRUCT_LOGICAL_AND, mchild2);
								} else {
									transform_nocopy(STRUCT_LOGICAL_OR, mchild2);
								}
								calculatesub(eo2, eo, false);
							}
							CHILDREN_UPDATED;
							if(stop_iv) {
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
								fix_intervals(*this, eo2);
							}
							return true;
						}
					}
				}
			}
			if(CALCULATOR->aborted()) return false;

			// x+x^(1/a)=b => x=(b-x)^a
			if(eo2.expand && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) && CHILD(0).size() <= 10 && CHILD(0).size() > 1) {
				int i_root = 0;
				size_t root_index = 0;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CALCULATOR->aborted()) return false;
					if(CHILD(0)[i].isPower()) {
						if(CHILD(0)[i][1].isNumber() && !CHILD(0)[i][1].isInteger() && CHILD(0)[i][1].number().numeratorIsOne() && CHILD(0)[i][1].number().denominatorIsLessThan(10)) {
							if(i_root) {
								if(i_root != CHILD(0)[i][1].number().denominator().intValue()) {
									i_root = 0;
									break;
								}
							} else {
								i_root = CHILD(0)[i][1].number().denominator().intValue();
								root_index = i;
							}
						} else if(!CHILD(0)[i][1].isNumber() || !CHILD(0)[i][1].number().isInteger()) {
							i_root = 0;
							break;
						}
					} else if(CHILD(0)[i].isFunction() && CHILD(0)[i].function() == CALCULATOR->f_root) {
						if(VALID_ROOT(CHILD(0)[i]) && CHILD(0)[i][1].number().isLessThan(10)) {
							if(i_root) {
								if(i_root != CHILD(0)[i][1].number().intValue()) {
									i_root = 0;
									break;
								}
							} else {
								i_root = CHILD(0)[i][1].number().intValue();
								root_index = i;
							}
						} else {
							i_root = 0;
							break;
						}
					} else if(CHILD(0)[i].isMultiplication()) {
						bool b_break = false;
						for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
							if(CHILD(0)[i][i2].contains(x_var)) {
								if(CHILD(0)[i][i2].isPower()) {
									if(CHILD(0)[i][i2][1].isNumber() && !CHILD(0)[i][i2][1].number().isInteger() && CHILD(0)[i][i2][1].number().numeratorIsOne() && CHILD(0)[i][i2][1].number().denominatorIsLessThan(10)) {
										if(i_root) {
											if(i_root != CHILD(0)[i][i2][1].number().denominator().intValue()) {
												i_root = 0;
												b_break = true;
												break;
											}
										} else {
											i_root = CHILD(0)[i][i2][1].number().denominator().intValue();
											root_index = i;
										}
									} else if(!CHILD(0)[i][i2][1].isNumber() || !CHILD(0)[i][i2][1].number().isInteger()) {
										i_root = 0;
										b_break = true;
										break;
									}
								} else if(CHILD(0)[i][i2].isFunction() && CHILD(0)[i][i2].function() == CALCULATOR->f_root) {
									if(VALID_ROOT(CHILD(0)[i][i2]) && CHILD(0)[i][i2][1].number().isLessThan(10)) {
										if(i_root) {
											if(i_root != CHILD(0)[i][i2][1].number().intValue()) {
												i_root = 0;
												b_break = true;
												break;
											}
										} else {
											i_root = CHILD(0)[i][i2][1].number().intValue();
											root_index = i;
										}
									} else {
										i_root = 0;
										b_break = true;
										break;
									}
								}
							}
						}
						if(b_break) break;
					}
				}
				if(i_root) {
					MathStructure mtest(*this);
					MathStructure msub;
					if(mtest[0].size() == 2) {
						msub = mtest[0][root_index == 0 ? 1 : 0];
					} else {
						msub = mtest[0];
						msub.delChild(root_index + 1);
					}
					mtest[0].setToChild(root_index + 1);
					mtest[1].calculateSubtract(msub, eo2);
					if(mtest[0].calculateRaise(i_root, eo2) && mtest[1].calculateRaise(i_root, eo2)) {
						mtest.childrenUpdated();
						if(mtest.isolate_x(eo2, eo, x_var)) {
							if(test_comparisons(*this, mtest, x_var, eo) >= 0) {
								set(mtest);
								return true;
							}
						}
					}
					
				}
			}

			// x+y/x=z => x*x+y=z*x
			MathStructure mdiv;
			mdiv.setUndefined();
			MathStructure mdiv_inv;
			bool b_multiple_div = false;
			for(size_t i = 0; i < CHILD(0).size() && !b_multiple_div; i++) {
				if(CALCULATOR->aborted()) return false;
				if(CHILD(0)[i].isMultiplication()) {
					for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
						if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number().isReal() && CHILD(0)[i][i2][1].number().isNegative()) {
							if(!mdiv.isUndefined()) b_multiple_div = true;
							else mdiv = CHILD(0)[i][i2];
							break;
						}
					}
				} else if(CHILD(0)[i].isPower() && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number().isReal() && CHILD(0)[i][1].number().isNegative()) {
					if(!mdiv.isUndefined()) b_multiple_div = true;
					else mdiv = CHILD(0)[i];
				}
			}
			if(!mdiv.isUndefined() && b_multiple_div && mdiv.containsInterval()) {
				MathStructure mbak(*this);
				CALCULATOR->beginTemporaryStopIntervalArithmetic();
				bool failed = false;
				fix_intervals(*this, eo2, &failed);
				if(!failed && isolate_x_sub(eo, eo2, x_var, morig)) {
					CALCULATOR->endTemporaryStopIntervalArithmetic();
					if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
					fix_intervals(*this, eo2);
					return true;
				}
				CALCULATOR->endTemporaryStopIntervalArithmetic();
				set(mbak);
			} else if(!mdiv.isUndefined()) {
				if(mdiv[1].isMinusOne()) {
					mdiv_inv = mdiv[0];
				} else {
					mdiv_inv = mdiv;
					mdiv_inv[1].number().negate();
				}
				EvaluationOptions eo3 = eo2;
				eo3.expand = true;
				bool b2 = false;
				PrintOptions po;
				po.interval_display = INTERVAL_DISPLAY_SIGNIFICANT_DIGITS;
				for(size_t i3 = 0; i3 < CHILD(0).size(); i3++) {
					bool b = false;
					if(!b2 || !mdiv.containsInterval()) {
						if(CHILD(0)[i3].isPower() && CHILD(0)[i3].equals(mdiv, true)) {
							CHILD(0)[i3].set(1, 1, 0, true);
							b = true;
						} else if(CHILD(0)[i3].isMultiplication()) {
							for(size_t i4 = 0; i4 < CHILD(0)[i3].size(); i4++) {
								if(CHILD(0)[i3][i4].isPower() && CHILD(0)[i3][i4].equals(mdiv, true)) {
									b = true;
									CHILD(0)[i3].delChild(i4 + 1);
									if(CHILD(0)[i3].size() == 0) CHILD(0)[i3].set(1, 1, 0, true);
									else if(CHILD(0)[i3].size() == 1) CHILD(0)[i3].setToChild(1, true);
									break;
								}
							}
						}
					}
					if(!b) {
						CHILD(0)[i3].calculateMultiply(mdiv_inv, eo3);
					} else {
						b2 = true;
					}
				}
				CHILD(0).childrenUpdated();
				CHILD(0).evalSort();
				CHILD(0).calculatesub(eo2, eo, false);
				CHILD(1).calculateMultiply(mdiv_inv, eo3);
				CHILDREN_UPDATED
				if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
					bool not_equals = ct_comp == COMPARISON_NOT_EQUALS;
					isolate_x(eo2, eo, x_var, false);
					if(!mdiv[0].representsNonZero(true)) {
						MathStructure *mtest = new MathStructure(mdiv[0]);
						mtest->add(m_zero, not_equals ? OPERATION_EQUALS : OPERATION_NOT_EQUALS);
						mtest->isolate_x_sub(eo, eo2, x_var);
						add_nocopy(mtest, not_equals ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND);
						calculatesub(eo2, eo, false);
					}
				} else {
					MathStructure *malt = new MathStructure(*this);
					if(ct_comp == COMPARISON_EQUALS_GREATER) {
						ct_comp = COMPARISON_EQUALS_LESS;
					} else if(ct_comp == COMPARISON_GREATER) {
						ct_comp = COMPARISON_LESS;
					} else if(ct_comp == COMPARISON_EQUALS_LESS) {
						ct_comp = COMPARISON_EQUALS_GREATER;
					} else if(ct_comp == COMPARISON_LESS) {
						ct_comp = COMPARISON_GREATER;
					}
					isolate_x(eo, eo2, x_var, morig);
					MathStructure *mtest = new MathStructure(mdiv);
					mtest->add(m_zero, OPERATION_LESS);
					MathStructure *mtest_alt = new MathStructure(*mtest);
					mtest_alt->setComparisonType(COMPARISON_GREATER);
					mtest->isolate_x_sub(eo, eo2, x_var);
					add_nocopy(mtest, OPERATION_LOGICAL_AND);
					calculatesub(eo2, eo, false);
					malt->isolate_x(eo, eo2, x_var, morig);
					mtest_alt->isolate_x_sub(eo, eo2, x_var);
					malt->add_nocopy(mtest_alt, OPERATION_LOGICAL_AND);
					malt->calculatesub(eo2, eo, false);
					add_nocopy(malt, OPERATION_LOGICAL_OR);
					calculatesub(eo2, eo, false);
					return true;
				}
				return true;
			}

			// x^a+x^(b/c)=d => x^(1/c)^(a*c)+x^(1/c)^b=d; f(x)^a+f(x)^b=c => u^a+u^b=c
			MathStructure mbase;
			b = true;
			Number nr_root;
			bool b_f_root = false;

			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(CALCULATOR->aborted()) return false;
				if(!mbase.isZero() && CHILD(0)[i] == mbase) {
				} else if(CHILD(0)[i].isPower() && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number().isPositive() && CHILD(0)[i][1].number().isRational()) {
					if(CHILD(0)[i][0].isFunction() && CHILD(0)[i][0].function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0)[i][0])) {
						if(mbase.isZero()) mbase = CHILD(0)[i][0][0];
						else if(mbase != CHILD(0)[i][0][0]) {b = false; break;}
						if(!nr_root.isZero()) {
							if(!b_f_root) {b = false; break;}
							nr_root.lcm(CHILD(0)[i][0][1].number());
						} else {
							nr_root = CHILD(0)[i][0][1].number();
						}
						b_f_root = true;
					} else {
						if(mbase.isZero()) mbase = CHILD(0)[i][0];
						else if(mbase != CHILD(0)[i][0]) {b = false; break;}
						if(!CHILD(0)[i][1].number().isInteger()) {
							if(b_f_root) {b = false; break;}
							if(!nr_root.isZero()) {
								nr_root.lcm(CHILD(0)[i][1].number().denominator());
							} else {
								nr_root = CHILD(0)[i][1].number().denominator();
							}
						}
					}
				} else if(CHILD(0)[i].isMultiplication()) {
					for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
						if(!mbase.isZero() && CHILD(0)[i][i2] == mbase) {
						} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number().isPositive() && CHILD(0)[i][i2][1].number().isRational()) {
							if(CHILD(0)[i][i2][0].isFunction() && CHILD(0)[i][i2][0].function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0)[i][i2][0])) {
								if(mbase.isZero()) mbase = CHILD(0)[i][i2][0][0];
								else if(mbase != CHILD(0)[i][i2][0][0]) {b = false; break;}
								if(!nr_root.isZero()) {
									if(!b_f_root) {b = false; break;}
									nr_root.lcm(CHILD(0)[i][i2][0][1].number());
								} else {
									nr_root = CHILD(0)[i][i2][0][1].number();
								}
								b_f_root = true;
							} else {				
								if(mbase.isZero()) mbase = CHILD(0)[i][i2][0];
								else if(mbase != CHILD(0)[i][i2][0]) {b = false; break;}
								if(!CHILD(0)[i][i2][1].number().isInteger()) {
									if(b_f_root) {b = false; break;}
									if(!nr_root.isZero()) {
										nr_root.lcm(CHILD(0)[i][i2][1].number().denominator());
									} else {
										nr_root = CHILD(0)[i][i2][1].number().denominator();
									}
								}
							}
						} else if(CHILD(0)[i][i2].isFunction() && CHILD(0)[i][i2].function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0)[i][i2])) {
							if(mbase.isZero()) mbase = CHILD(0)[i][i2][0];
							else if(mbase != CHILD(0)[i][i2][0]) {b = false; break;}
							if(!nr_root.isZero()) {
								if(!b_f_root) {b = false; break;}
								nr_root.lcm(CHILD(0)[i][i2][1].number());
							} else {
								nr_root = CHILD(0)[i][i2][1].number();
							}
							b_f_root = true;
						} else if(CHILD(0)[i][i2].isNumber() && !CHILD(0)[i][i2].number().isZero()) {
						} else if(mbase.isZero()) {
							mbase = CHILD(0)[i][i2];
						} else {
							b = false; break;
						}
					}
					if(!b) break;
				} else if(CHILD(0)[i].isFunction() && CHILD(0)[i].function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0)[i])) {
					if(mbase.isZero()) mbase = CHILD(0)[i][0];
					else if(mbase != CHILD(0)[i][0]) {b = false; break;}
					if(!nr_root.isZero()) {
						if(!b_f_root) {b = false; break;}
						nr_root.lcm(CHILD(0)[i][1].number());
					} else {
						nr_root = CHILD(0)[i][1].number();
					}
					b_f_root = true;
				} else if(mbase.isZero()) {
					mbase = CHILD(0)[i];
				} else {
					b = false; break;
				}
			}
			if(b && !mbase.isZero() && nr_root.isLessThan(100)) {
				MathStructure mvar(mbase);
				if(!nr_root.isZero()) {
					if(b_f_root) {
						MathStructure mroot(nr_root);
						mvar.set(CALCULATOR->f_root, &mbase, &mroot, NULL);
					} else {
						Number nr_pow(nr_root);
						nr_pow.recip();
						mvar.raise(nr_pow);
					}
				}
				UnknownVariable *var = NULL;
				if(mvar != x_var) {
					var = new UnknownVariable("", string(LEFT_PARENTHESIS) + mvar.print() + RIGHT_PARENTHESIS);
					var->setInterval(mvar);
				}
				if(!nr_root.isZero()) {
					MathStructure mbak(*this);
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CALCULATOR->aborted()) {set(mbak); return false;}
						if(CHILD(0)[i] == mbase) {
							CHILD(0)[i] = var;
							CHILD(0)[i].raise(nr_root);
						} else if(CHILD(0)[i].isPower() && CHILD(0)[i][1].isNumber()) {
							if(CHILD(0)[i][0].isFunction() && CHILD(0)[i][0].function() == CALCULATOR->f_root && CHILD(0)[i][0] != mbase) {
								CHILD(0)[i][1].number().multiply(nr_root);
								CHILD(0)[i][1].number().divide(CHILD(0)[i][0][1].number());
								if(CHILD(0)[i][1].isOne()) CHILD(0)[i] = var;
								else CHILD(0)[i][0] = var;
							} else if(CHILD(0)[i][0] == mbase) {
								CHILD(0)[i][1].number().multiply(nr_root);
								if(CHILD(0)[i][1].isOne()) CHILD(0)[i] = var;
								else CHILD(0)[i][0] = var;
							} else {
								// should not happen
							}
						} else if(CHILD(0)[i].isMultiplication()) {
							for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
								if(CHILD(0)[i][i2] == mbase) {
									CHILD(0)[i][i2] = var;
									CHILD(0)[i][i2].raise(nr_root);
								} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][1].isNumber()) {
									if(CHILD(0)[i][i2][0].isFunction() && CHILD(0)[i][i2][0].function() == CALCULATOR->f_root && CHILD(0)[i][i2][0] != mbase) {
										CHILD(0)[i][i2][1].number().multiply(nr_root);
										CHILD(0)[i][i2][1].number().divide(CHILD(0)[i][i2][0][1].number());
										if(CHILD(0)[i][i2][1].isOne()) CHILD(0)[i][i2] = var;
										else CHILD(0)[i][i2][0] = var;
									} else if(CHILD(0)[i][i2][0] == mbase) {
										CHILD(0)[i][i2][1].number().multiply(nr_root);
										if(CHILD(0)[i][i2][1].isOne()) CHILD(0)[i][i2] = var;
										else CHILD(0)[i][i2][0] = var;
									} else {
										// should not happen
									}
								} else if(CHILD(0)[i][i2].isFunction() && CHILD(0)[i][i2].function() == CALCULATOR->f_root) {
									Number nr_pow(nr_root);
									nr_pow.divide(CHILD(0)[i][i2][1].number());
									CHILD(0)[i][i2][1] = nr_pow;
									CHILD(0)[i][i2][0] = var;
									CHILD(0)[i][i2].setType(STRUCT_POWER);
								} else if(CHILD(0)[i][i2].isNumber() && !CHILD(0)[i][i2].number().isZero()) {
								} else {
									// should not happen
								}
							}
						} else if(CHILD(0)[i].isFunction() && CHILD(0)[i].function() == CALCULATOR->f_root) {
							Number nr_pow(nr_root);
							nr_pow.divide(CHILD(0)[i][1].number());
							CHILD(0)[i][1] = nr_pow;
							CHILD(0)[i][0] = var;
							CHILD(0)[i].setType(STRUCT_POWER);
						} else {
							// should not happen
						}
					}
					MathStructure u_var(var);
					replace(mvar, u_var);
					CHILD(0).calculatesub(eo2, eo2, true);
					CHILD_UPDATED(0)
					b = isolate_x_sub(eo, eo2, u_var);
					calculateReplace(u_var, mvar, eo2);
					var->destroy();
					if(b) isolate_x(eo, eo2, x_var);
					return b;
				} else if(mvar != x_var) {
					MathStructure u_var(var);
					replace(mvar, u_var);
					CHILD(0).calculatesub(eo2, eo2, true);
					CHILD_UPDATED(0)
					b = isolate_x_sub(eo, eo2, u_var);
					calculateReplace(u_var, mvar, eo2);
					var->destroy();
					if(b) isolate_x(eo, eo2, x_var);
					return b;
				}
			}

			if(!eo2.expand) break;

			// Try factorization
			if(!morig || !equals(*morig)) {
				MathStructure mtest(*this);
				if(!mtest[1].isZero()) {
					mtest[0].calculateSubtract(CHILD(1), eo2);
					mtest[1].clear();
					mtest.childrenUpdated();
				}
				if(mtest[0].factorize(eo2, false, false, 0, false, false) && !(mtest[0].isMultiplication() && mtest[0].size() == 2 && ((mtest[0][0].isNumber() && mtest[0][1].isAddition()) || mtest[0][0] == CHILD(1) || mtest[0][1] == CHILD(1)))) {
					mtest.childUpdated(1);
					if(mtest.isolate_x_sub(eo, eo2, x_var, this)) {
						set_nocopy(mtest);
						return true;
					}
				}
			}

			// ax^3+bx^2+cx=d
			if(CHILD(0).size() >= 2 && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
				bool cbpow = false, sqpow = false, nopow = false;
				MathStructure mstruct_a, mstruct_b, mstruct_c;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CALCULATOR->aborted()) return false;
					if(CHILD(0)[i].isPower() && CHILD(0)[i][0] == x_var && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number() == 3) {
						if(cbpow) mstruct_a.add(m_one, true);
						else {cbpow = true; mstruct_a.set(m_one);}
					} else if(CHILD(0)[i].isPower() && CHILD(0)[i][0] == x_var && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number() == 2) {
						if(sqpow) mstruct_b.add(m_one, true);
						else {sqpow = true; mstruct_b.set(m_one);}
					} else if(CHILD(0)[i] == x_var) {
						if(nopow) mstruct_c.add(m_one, true);
						else {nopow = true; mstruct_c.set(m_one);}
					} else if(CHILD(0)[i].isMultiplication()) {
						for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
							if(CHILD(0)[i][i2] == x_var) {
								if(nopow) {
									MathStructure *madd = new MathStructure(CHILD(0)[i]);
									madd->delChild(i2 + 1, true);
									mstruct_c.add_nocopy(madd, true);
								} else {
									nopow = true; 
									mstruct_c = CHILD(0)[i];
									mstruct_c.delChild(i2 + 1, true);
								}
							} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][0] == x_var && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number() == 2) {
								if(sqpow) {
									MathStructure *madd = new MathStructure(CHILD(0)[i]);
									madd->delChild(i2 + 1, true);
									mstruct_b.add_nocopy(madd, true);
								} else {
									sqpow = true;
									mstruct_b = CHILD(0)[i];
									mstruct_b.delChild(i2 + 1, true);
								}
							} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][0] == x_var && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number() == 3) {
								if(cbpow) {
									MathStructure *madd = new MathStructure(CHILD(0)[i]);
									madd->delChild(i2 + 1, true);
									mstruct_a.add_nocopy(madd, true);
								} else {
									cbpow = true;
									mstruct_a = CHILD(0)[i];
									mstruct_a.delChild(i2 + 1, true);
								}
							} else if(CHILD(0)[i][i2].contains(x_var)) {
								cbpow = false;
								sqpow = false;
								nopow = false;
								break;
							}
						}
						if(!cbpow && !sqpow && !nopow) break;
					} else {
						cbpow = false;
						sqpow = false;
						nopow = false;
						break;
					}
				}
				bool b = false;
				if(cbpow && (nopow || sqpow) && !mstruct_a.representsZero(true)) {
					b = mstruct_a.representsNonZero(true);
					if(!b && eo2.approximation == APPROXIMATION_EXACT) {
						MathStructure mtest(mstruct_a);
						mtest.add(m_zero, OPERATION_NOT_EQUALS);
						EvaluationOptions eo3 = eo2;
						eo3.test_comparisons = true;
						mtest.calculatesub(eo3, eo, false);
						b = mtest.isOne();
					}
				}
				if(b) {
				
					MathStructure mbak(*this);
					
					CALCULATOR->beginTemporaryStopIntervalArithmetic();
					bool failed = false;
					fix_intervals(*this, eo2, &failed);
					if(failed) {
						set(mbak);
						CALCULATOR->endTemporaryStopIntervalArithmetic();
					} else {

						EvaluationOptions eo3 = eo2;
						eo3.keep_zero_units = false;
						
						MathStructure mstruct_d(CHILD(1));
						mstruct_d.calculateNegate(eo2);
						
						// 18abcd - 4b^3*d + b^2*c^2 - 4a*c^3 - 27a^2*d^2
						MathStructure mdelta(18, 1, 0);
						mdelta.multiply(mstruct_a);
						mdelta.multiply(mstruct_b, true);
						mdelta.multiply(mstruct_c, true);
						mdelta.multiply(mstruct_d, true);
						MathStructure *mdelta_b = new MathStructure(mstruct_b);
						mdelta_b->raise(nr_three);
						mdelta_b->multiply(Number(-4, 1, 0));
						mdelta_b->multiply(mstruct_d);
						MathStructure *mdelta_c = new MathStructure(mstruct_b);
						mdelta_c->raise(nr_two);
						MathStructure *mdelta_c2 = new MathStructure(mstruct_c);
						mdelta_c2->raise(nr_two);
						mdelta_c->multiply_nocopy(mdelta_c2);
						MathStructure *mdelta_d = new MathStructure(mstruct_c);
						mdelta_d->raise(nr_three);
						mdelta_d->multiply(Number(-4, 1, 0));
						mdelta_d->multiply(mstruct_a, true);
						MathStructure *mdelta_e = new MathStructure(mstruct_a);
						mdelta_e->raise(nr_two);
						MathStructure *mdelta_e2 = new MathStructure(mstruct_d);
						mdelta_e2->raise(nr_two);
						mdelta_e->multiply_nocopy(mdelta_e2);
						mdelta_e->multiply(Number(-27, 1, 0));
						mdelta.add_nocopy(mdelta_b);
						mdelta.add_nocopy(mdelta_c, true);
						mdelta.add_nocopy(mdelta_d, true);
						mdelta.add_nocopy(mdelta_e, true);
						mdelta.calculatesub(eo3, eo);
						
						if(CALCULATOR->aborted()) {CALCULATOR->endTemporaryStopIntervalArithmetic(); set(mbak); return false;}
						
						int b_zero = -1;
						int b_real = -1;
						if(mdelta.representsNonZero(true)) b_zero = 0;
						else if(mdelta.representsZero(true)) b_zero = 1;
						if(b_zero == 0) {
							if(mdelta.representsPositive(true)) {
								b_real = 1;
							} else if(mdelta.representsNegative(true)) {
								b_real = 0;
							}
						} else if(b_zero < 0) {
							ComparisonResult cr = mdelta.compareApproximately(m_zero, eo);
							if(cr == COMPARISON_RESULT_EQUAL) {
								b_zero = 1;
							} else if(COMPARISON_IS_NOT_EQUAL(cr)) {
								if(cr == COMPARISON_RESULT_LESS) b_real = 0;
								else if(cr == COMPARISON_RESULT_GREATER) b_real = 1;
								b_zero = 0;
							}
						}
						if(b_real < 0 && mdelta.representsComplex(true)) b_real = -2;
						if(b_real == -1) b_zero = -1;
						
						MathStructure mdelta0;
						int b0_zero = -1;
						
						if(CALCULATOR->aborted()) {CALCULATOR->endTemporaryStopIntervalArithmetic(); set(mbak); return false;}
						
						if(b_zero >= 0) {
							// b^2 - 3ac
							mdelta0 = mstruct_b;
							mdelta0.raise(nr_two);
							MathStructure *mdelta0_b = new MathStructure(-3, 1, 0);
							mdelta0_b->multiply(mstruct_a);
							mdelta0_b->multiply(mstruct_c, true);
							mdelta0.add_nocopy(mdelta0_b);
							mdelta0.calculatesub(eo3, eo, true);
							if(mdelta0.representsNonZero(true)) b0_zero = 0;
							else if(mdelta0.representsZero(true)) b0_zero = 1;
							else {
								ComparisonResult cr = mdelta0.compareApproximately(m_zero, eo);
								if(cr == COMPARISON_RESULT_EQUAL) {
									b0_zero = 1;
								} else if(COMPARISON_IS_NOT_EQUAL(cr)) {
									b0_zero = 0;
								}
							}
						}
						
						if(CALCULATOR->aborted()) {CALCULATOR->endTemporaryStopIntervalArithmetic(); set(mbak); return false;}
						
						if(b_zero == 1) {
							if(b0_zero == 1) {
								// -b/(3a)
								CHILD(0) = x_var;
								CHILD(1).set(-1, 3);
								CHILD(1).calculateMultiply(mstruct_b, eo3);
								CHILD(1).calculateDivide(mstruct_a, eo3);
								CHILDREN_UPDATED;
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
								fix_intervals(*this, eo2);
								return true;
							} else if(b0_zero == 0) {
								MathStructure *malt1 = new MathStructure(x_var);
								malt1->transform(STRUCT_COMPARISON, m_zero);
								malt1->setComparisonType(comparisonType());
								MathStructure *malt2 = new MathStructure(*malt1);
								
								// (9ad - bc) / (2*delta0)
								(*malt1)[1].set(9, 1, 0);
								(*malt1)[1].calculateMultiply(mstruct_a, eo3);
								(*malt1)[1].calculateMultiply(mstruct_d, eo3);
								MathStructure *malt1_2b = new MathStructure(-1, 1, 0);
								malt1_2b->calculateMultiply(mstruct_b, eo3);
								malt1_2b->calculateMultiply(mstruct_c, eo3);
								(*malt1)[1].add_nocopy(malt1_2b);
								(*malt1)[1].calculateAddLast(eo3);
								(*malt1)[1].calculateDivide(nr_two, eo3);
								(*malt1)[1].calculateDivide(mdelta0, eo3);
								
								// (4abc - 9a^2*d - b^3) / (a*delta0)
								(*malt2)[1].set(4, 1, 0);
								(*malt2)[1].calculateMultiply(mstruct_a, eo3);
								(*malt2)[1].calculateMultiply(mstruct_b, eo3);
								(*malt2)[1].calculateMultiply(mstruct_c, eo3);
								MathStructure *malt2_2b = new MathStructure(mstruct_a);
								malt2_2b->calculateRaise(nr_two, eo3);
								malt2_2b->calculateMultiply(Number(-9, 1, 0), eo3);
								malt2_2b->calculateMultiply(mstruct_d, eo3);
								(*malt2)[1].add_nocopy(malt2_2b);
								(*malt2)[1].calculateAddLast(eo3);
								MathStructure *malt2_2c = new MathStructure(mstruct_b);
								malt2_2c->calculateRaise(nr_three, eo3);
								malt2_2c->calculateNegate(eo3);
								(*malt2)[1].add_nocopy(malt2_2c);
								(*malt2)[1].calculateAddLast(eo3);
								(*malt2)[1].calculateDivide(mstruct_a, eo3);
								(*malt2)[1].calculateDivide(mdelta0, eo3);
								
								if(ct_comp == COMPARISON_NOT_EQUALS) {
									clear(true);
									setType(STRUCT_LOGICAL_AND);
								} else {
									clear(true);
									setType(STRUCT_LOGICAL_OR);
								}
								
								malt1->childUpdated(2);
								malt2->childUpdated(2);
								
								addChild_nocopy(malt1);
								addChild_nocopy(malt2);
								
								calculatesub(eo2, eo, false);
								
								CALCULATOR->endTemporaryStopIntervalArithmetic();
								if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
								fix_intervals(*this, eo2);

								return true;
							}
						}
						
						if(CALCULATOR->aborted()) return false;
						
						MathStructure mdelta1;
						bool b_neg = false;
						
						if(b_zero == 0 && b0_zero >= 0) {

							// 2b^3 - 9abc + 27a^2*d
							mdelta1 = mstruct_b;
							mdelta1.raise(nr_three);
							mdelta1.multiply(nr_two);
							MathStructure *mdelta1_b = new MathStructure(-9, 1, 0);
							mdelta1_b->multiply(mstruct_a);
							mdelta1_b->multiply(mstruct_b, true);
							mdelta1_b->multiply(mstruct_c, true);
							MathStructure *mdelta1_c = new MathStructure(mstruct_a);
							mdelta1_c->raise(nr_two);
							mdelta1_c->multiply(Number(27, 1, 0));
							mdelta1_c->multiply(mstruct_d, true);
							mdelta1.add_nocopy(mdelta1_b);
							mdelta1.add_nocopy(mdelta1_c, true);
							mdelta1.calculatesub(eo3, eo, true);
							
							if(b0_zero == 1) {
								if(mdelta1.representsNegative(true)) {
									b_neg = true;
								} else if(!mdelta1.representsPositive(true)) {
									if(mdelta1.representsZero(true)) {
										// -b/(3a)
										CHILD(0) = x_var;
										CHILD(1).set(-1, 3);
										CHILD(1).calculateMultiply(mstruct_b, eo3);
										CHILD(1).calculateDivide(mstruct_a, eo3);
										CHILDREN_UPDATED;
										CALCULATOR->endTemporaryStopIntervalArithmetic();
										if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
										fix_intervals(*this, eo2);
										return true;
										return true;
									}
									ComparisonResult cr = mdelta1.compareApproximately(m_zero, eo);
									if(cr == COMPARISON_RESULT_EQUAL) {
										// -b/(3a)
										CHILD(0) = x_var;
										CHILD(1).set(-1, 3);
										CHILD(1).calculateMultiply(mstruct_b, eo3);
										CHILD(1).calculateDivide(mstruct_a, eo3);
										CHILDREN_UPDATED;
										CALCULATOR->endTemporaryStopIntervalArithmetic();
										if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
										fix_intervals(*this, eo2);
										return true;
									} else if(cr == COMPARISON_RESULT_LESS) {
										b_neg = true;
									} else if(cr != COMPARISON_RESULT_GREATER) {
										b_zero = -1;
									}
								}
							}
						
						}
						
						if(b_zero == 0 && b0_zero == 0) {
						
							// ((delta1 +-sqrt(delta1^2-4*delta0^3))/2)^(1/3)
							MathStructure mC;
							MathStructure *md0_43 = new MathStructure(mdelta0);
							md0_43->raise(nr_three);
							md0_43->multiply(Number(-4, 1, 0));
							MathStructure *md1_2 = new MathStructure(mdelta1);
							md1_2->raise(nr_two);
							md1_2->add_nocopy(md0_43);
							md1_2->raise(nr_half);
							if(b_neg) md1_2->calculateNegate(eo3);
							md1_2->calculatesub(eo3, eo, true); 
							
							if(CALCULATOR->aborted()) {CALCULATOR->endTemporaryStopIntervalArithmetic(); set(mbak); return false;}
						
							mC = mdelta1;
							mC.add_nocopy(md1_2);
							mC.calculateAddLast(eo3);
							mC.calculateDivide(nr_two, eo3);
							if(b_real == 0 && x_var.representsReal(true)) {
								if(!mC.representsComplex(true)) {
									mC.transform(STRUCT_FUNCTION);
									mC.addChild(nr_three);
									mC.setFunction(CALCULATOR->f_root);
									mC.calculateFunctions(eo);
									// x = -1/(3a)*(b + C + delta0/C)
									CHILD(0) = x_var;
									CHILD(1).set(-1, 3, 0);
									CHILD(1).calculateDivide(mstruct_a, eo3);
									MathStructure *malt1_2b = new MathStructure(mdelta0);
									malt1_2b->calculateDivide(mC, eo3);
									malt1_2b->calculateAdd(mC, eo3);
									malt1_2b->calculateAdd(mstruct_b, eo3);
									CHILD(1).multiply_nocopy(malt1_2b);
									CHILD(1).calculateMultiplyLast(eo3);
									CHILDREN_UPDATED;
									CALCULATOR->endTemporaryStopIntervalArithmetic();
									if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
									fix_intervals(*this, eo2);
									return true;
								}
							} else if(eo3.allow_complex) {
								
								if(mC.representsNegative(true)) {
									mC.calculateNegate(eo3);
									mC.calculateRaise(Number(1, 3, 0), eo3);
									mC.calculateNegate(eo3);
								} else {
									mC.calculateRaise(Number(1, 3, 0), eo3);
								}
							
								// x = -1/(3a)*(b + C + delta0/C)
							
								MathStructure *malt1 = new MathStructure(x_var);
								malt1->transform(STRUCT_COMPARISON, Number(-1, 3, 0));
								malt1->setComparisonType(comparisonType());
								(*malt1)[1].calculateDivide(mstruct_a, eo3);
								
								MathStructure *malt2 = new MathStructure(*malt1);
								MathStructure *malt3 = new MathStructure(*malt1);

								MathStructure *malt1_2b = new MathStructure(mdelta0);
								malt1_2b->calculateDivide(mC, eo3);
								malt1_2b->calculateAdd(mC, eo3);
								malt1_2b->calculateAdd(mstruct_b, eo3);
								(*malt1)[1].multiply_nocopy(malt1_2b);
								(*malt1)[1].calculateMultiplyLast(eo3);
								
								MathStructure cbrt_mul(nr_three);
								cbrt_mul.calculateRaise(nr_half, eo3);
								cbrt_mul.calculateMultiply(nr_one_i, eo3);
								MathStructure cbrt_mul2(cbrt_mul);
								cbrt_mul.calculateMultiply(nr_half, eo3);
								cbrt_mul.calculateAdd(Number(-1, 2, 0), eo3);
								cbrt_mul2.calculateMultiply(Number(-1, 2, 0), eo3);
								cbrt_mul2.calculateAdd(Number(-1, 2, 0), eo3);
								
								MathStructure mC2(mC);
								mC2.calculateMultiply(cbrt_mul, eo3);
								MathStructure mC3(mC);
								mC3.calculateMultiply(cbrt_mul2, eo3);
								
								MathStructure *malt2_2b = new MathStructure(mdelta0);
								malt2_2b->calculateDivide(mC2, eo3);
								malt2_2b->calculateAdd(mC2, eo3);
								malt2_2b->calculateAdd(mstruct_b, eo3);
								(*malt2)[1].multiply_nocopy(malt2_2b);
								(*malt2)[1].calculateMultiplyLast(eo3);
								
								MathStructure *malt3_2b = new MathStructure(mdelta0);
								malt3_2b->calculateDivide(mC3, eo3);
								malt3_2b->calculateAdd(mC3, eo3);
								malt3_2b->calculateAdd(mstruct_b, eo3);
								(*malt3)[1].multiply_nocopy(malt3_2b);
								(*malt3)[1].calculateMultiplyLast(eo3);
								
								if(b_real == 1) {
									if((*malt1)[1].isNumber()) {
										(*malt1)[1].number().clearImaginary();
									} else if((*malt1)[1].isMultiplication() && (*malt1)[1][0].isNumber()) {
										bool b = true;
										for(size_t i = 1; i < (*malt1)[1].size(); i++) {
											if(!(*malt1)[1][i].representsReal(true)) {
												b = false;
												break;
											}
										}
										if(b) (*malt1)[1][0].number().clearImaginary();
									}
									if((*malt2)[1].isNumber()) {
										(*malt2)[1].number().clearImaginary();
									} else if((*malt2)[1].isMultiplication() && (*malt2)[1][0].isNumber()) {
										bool b = true;
										for(size_t i = 1; i < (*malt2)[1].size(); i++) {
											if(!(*malt2)[1][i].representsReal(true)) {
												b = false;
												break;
											}
										}
										if(b) (*malt2)[1][0].number().clearImaginary();
									}
									if((*malt3)[1].isNumber()) {
										(*malt3)[1].number().clearImaginary();
									} else if((*malt3)[1].isMultiplication() && (*malt3)[1][0].isNumber()) {
										bool b = true;
										for(size_t i = 1; i < (*malt3)[1].size(); i++) {
											if(!(*malt3)[1][i].representsReal(true)) {
												b = false;
												break;
											}
										}
										if(b) (*malt3)[1][0].number().clearImaginary();
									}
								}
								
								if(b_real < 1 || !x_var.representsReal(true) || (!(*malt1)[1].representsComplex(true) && !(*malt2)[1].representsComplex(true) && !(*malt3)[1].representsComplex(true))) {
								
									if(ct_comp == COMPARISON_NOT_EQUALS) {
										clear(true);
										setType(STRUCT_LOGICAL_AND);
									} else {
										clear(true);
										setType(STRUCT_LOGICAL_OR);
									}
									
									malt1->childUpdated(2);
									malt2->childUpdated(2);
									malt3->childUpdated(2);
								
									addChild_nocopy(malt1);
									addChild_nocopy(malt2);
									addChild_nocopy(malt3);
								
									calculatesub(eo2, eo, false);
									
									CALCULATOR->endTemporaryStopIntervalArithmetic();
									if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
									fix_intervals(*this, eo2);
								
									return true;
								}
							}
						}
					}
				}
			}

			// abs(x)+x=a => -x+x=a || x+x=a; sgn(x)+x=a => -1+x=a || 0+x=a || 1+x=a
			if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
				MathStructure *m = find_abs_sgn(CHILD(0), x_var);
				if(m && m->function() == CALCULATOR->f_abs) {

					MathStructure mabs(*m);

					ComparisonType cmp_type = ct_comp;
					MathStructure *malt = new MathStructure(*this);
					MathStructure mabs_minus(mabs[0]);
					mabs_minus.calculateNegate(eo2);
					(*malt)[0].replace(mabs, mabs_minus, mabs.containsInterval());
					(*malt)[0].calculatesub(eo2, eo, true);
					malt->childUpdated(1);
					MathStructure *mcheck = new MathStructure(mabs[0]);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_EQUALS_GREATER : OPERATION_LESS);
					mcheck->isolate_x_sub(eo, eo2, x_var);
					malt->isolate_x_sub(eo, eo2, x_var);
					malt->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					malt->calculatesub(eo2, eo, false);
					
					mcheck = new MathStructure(mabs[0]);
					CHILD(0).replace(mabs, mabs[0], mabs.containsInterval());
					CHILD(0).calculatesub(eo2, eo, true);
					CHILD_UPDATED(0)
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LESS : OPERATION_EQUALS_GREATER);
					mcheck->isolate_x_sub(eo, eo2, x_var);
					isolate_x_sub(eo, eo2, x_var);
					add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					calculatesub(eo2, eo, false);
					add_nocopy(malt, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					calculatesub(eo2, eo, false);

					return true;
				} else if(m && m->function() == CALCULATOR->f_signum) {
					MathStructure mabs(*m);

					ComparisonType cmp_type = ct_comp;
					MathStructure *malt = new MathStructure(*this);
					(*malt)[0].replace(mabs, m_minus_one, mabs.containsInterval());
					(*malt)[0].calculatesub(eo2, eo, true);
					malt->childUpdated(1);
					MathStructure *mcheck = new MathStructure(mabs[0]);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? ((*m)[1].isMinusOne() ? OPERATION_GREATER : OPERATION_EQUALS_GREATER) : ((*m)[1].isMinusOne() ? OPERATION_EQUALS_LESS : OPERATION_LESS));
					mcheck->isolate_x_sub(eo, eo2, x_var);
					malt->isolate_x_sub(eo, eo2, x_var);
					malt->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					malt->calculatesub(eo2, eo, false);
					
					MathStructure *malt0 = NULL;
					if(!(*m)[1].isOne() && !(*m)[1].isMinusOne()) {
						malt0 = new MathStructure(*this);
						(*malt0)[0].replace(mabs, (*m)[1], mabs.containsInterval());
						(*malt0)[0].calculatesub(eo2, eo, true);
						malt0->childUpdated(1);
						mcheck = new MathStructure(mabs[0]);
						mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_NOT_EQUALS : OPERATION_EQUALS);
						mcheck->isolate_x_sub(eo, eo2, x_var);
						malt0->isolate_x_sub(eo, eo2, x_var);
						malt0->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
						malt0->calculatesub(eo2, eo, false);
					}
					
					mcheck = new MathStructure(mabs[0]);
					CHILD(0).replace(mabs, m_one, mabs.containsInterval());
					CHILD(0).calculatesub(eo2, eo, true);
					CHILD_UPDATED(0)
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? ((*m)[1].isOne() ? OPERATION_LESS : OPERATION_EQUALS_LESS) : ((*m)[1].isOne() ? OPERATION_EQUALS_GREATER : OPERATION_GREATER));
					mcheck->isolate_x_sub(eo, eo2, x_var);
					isolate_x_sub(eo, eo2, x_var);
					add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					calculatesub(eo2, eo, false);
					add_nocopy(malt, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					if(malt0) add_nocopy(malt0, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					calculatesub(eo2, eo, false);
					return true;
				}
			}

			// Use newton raphson to calculate approximate solution for polynomial
			MathStructure x_value;
			if((ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) && CHILD(1).isNumber() && eo.approximation != APPROXIMATION_EXACT && !x_var.representsComplex(true)) {
				MathStructure mtest(CHILD(0));
				if(!CHILD(0).isZero()) mtest.calculateSubtract(CHILD(1), eo2);
				CALCULATOR->beginTemporaryStopIntervalArithmetic();
				bool failed = false;
				fix_intervals(mtest, eo2, &failed);
				if(failed) {
					CALCULATOR->endTemporaryStopIntervalArithmetic();
				} else {
					MathStructure mbak(*this);
					int ret = -1;
					EvaluationOptions eo3 = eo2;
					eo3.approximation = APPROXIMATION_APPROXIMATE;
					if(ct_comp == COMPARISON_EQUALS) clear(true);
					else set(1, 1, 0, true);
					while((ret = newton_raphson(mtest, x_value, x_var, eo3)) > 0) {
						if(isNumber()) {
							set(x_var, true);
							transform(STRUCT_COMPARISON, x_value);
							setComparisonType(mbak.comparisonType());
						} else {
							if(isComparison()) transform(mbak.comparisonType() == COMPARISON_NOT_EQUALS ? STRUCT_LOGICAL_AND : STRUCT_LOGICAL_OR);
							MathStructure *mnew = new MathStructure(x_var);
							mnew->transform(STRUCT_COMPARISON, x_value);
							mnew->setComparisonType(mbak.comparisonType());
							addChild_nocopy(mnew);
						}
						MathStructure mdiv(x_var);
						mdiv.calculateSubtract(x_value, eo3);
						MathStructure mtestcopy(mtest);
						MathStructure mrem;
						if(!polynomial_long_division(mtestcopy, mdiv, x_var, mtest, mrem, eo3, false, true) || !mrem.isNumber()) {
							ret = -1;
							break;
						}
						if(!mtest.contains(x_var)) break;
					}
					CALCULATOR->endTemporaryStopIntervalArithmetic();
					if(ret < 0) {
						set(mbak);
					} else {
						if(!x_var.representsReal()) CALCULATOR->error(false, _("Not all complex roots were calculated for %s."), mbak.print().c_str(), NULL);
						if((CALCULATOR->usesIntervalArithmetic() && eo.approximation != APPROXIMATION_EXACT) || mbak.containsInterval()) CALCULATOR->error(false, _("Interval arithmetic was disabled during calculation of %s."), mbak.print().c_str(), NULL);
						fix_intervals(*this, eo2);
						return true;
					}
				}
			}
			// Try factorization
			if((!morig || !equals(*morig)) && !CHILD(1).isZero()) {
				MathStructure mtest(*this);
				if(mtest[0].factorize(eo2, false, false, 0, false, false) && !(mtest[0].isMultiplication() && mtest[0].size() == 2 && (mtest[0][0].isNumber() || mtest[0][0] == CHILD(1) || mtest[0][1] == CHILD(1)))) {
					mtest.childUpdated(1);
					if(mtest.isolate_x_sub(eo, eo2, x_var, this)) {
						set_nocopy(mtest);
						return true;
					}
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(!representsNonMatrix()) return false;
			//x*y=0 => x=0 || y=0
			if(CHILD(1).isZero()) {
				if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
					vector<int> checktype;
					bool bdoit = false;
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CALCULATOR->aborted()) return false;
						if(CHILD(0)[i].isPower() && !CHILD(0)[i][1].representsNonNegative()) {
							if(CHILD(0)[i][1].representsNegative()) {
								checktype.push_back(1);
							} else {
								checktype.push_back(2);
								bdoit = true;
							}
						} else {
							checktype.push_back(0);
							bdoit = true;
						}
					}
					if(!bdoit) break;
					MathStructure *mcheckpowers = NULL;
					ComparisonType ct = ct_comp;
					setToChild(1);
					if(ct == COMPARISON_NOT_EQUALS) {
						setType(STRUCT_LOGICAL_AND);
					} else {
						setType(STRUCT_LOGICAL_OR);
					}
					MathStructure mbak(*this);
					for(size_t i = 0; i < SIZE;) {
						if(CALCULATOR->aborted()) {
							set(mbak);
							if(mcheckpowers) mcheckpowers->unref();
							return false;
						}
						if(checktype[i] > 0) {
							MathStructure *mcheck = new MathStructure(CHILD(i)[0]);
							if(ct_comp == COMPARISON_NOT_EQUALS) mcheck->add(m_zero, OPERATION_EQUALS);
							else mcheck->add(m_zero, OPERATION_NOT_EQUALS);
							mcheck->isolate_x_sub(eo, eo2, x_var);
							if(checktype[i] == 2) {
								MathStructure *mexpcheck = new MathStructure(CHILD(i)[1]);
								if(ct_comp == COMPARISON_NOT_EQUALS) mexpcheck->add(m_zero, OPERATION_LESS);
								else mexpcheck->add(m_zero, OPERATION_EQUALS_GREATER);
								mexpcheck->isolate_x_sub(eo, eo2, x_var);
								if(ct_comp == COMPARISON_NOT_EQUALS) mexpcheck->add_nocopy(mcheck, OPERATION_LOGICAL_AND, true);
								else mexpcheck->add_nocopy(mcheck, OPERATION_LOGICAL_OR, true);
								mexpcheck->calculatesub(eo2, eo, false);
								mcheck = mexpcheck;
							}
							if(mcheckpowers) {
								if(ct_comp == COMPARISON_NOT_EQUALS) mcheckpowers->add_nocopy(mcheck, OPERATION_LOGICAL_OR, true);
								else mcheckpowers->add_nocopy(mcheck, OPERATION_LOGICAL_AND, true);
							} else {
								mcheckpowers = mcheck;
							}
						}
						if(checktype[i] == 1) {
							ERASE(i)
							checktype.erase(checktype.begin() + i);
						} else {
							CHILD(i).transform(STRUCT_COMPARISON, m_zero);
							CHILD(i).setComparisonType(ct);
							CHILD(i).isolate_x_sub(eo, eo2, x_var);
							CHILD_UPDATED(i)
							i++;
						}
					}					
					if(SIZE == 1) setToChild(1);
					else if(SIZE == 0) clear(true);
					else calculatesub(eo2, eo, false);
					if(mcheckpowers) {
						mcheckpowers->calculatesub(eo2, eo, false);
						if(ct_comp == COMPARISON_NOT_EQUALS) add_nocopy(mcheckpowers, OPERATION_LOGICAL_OR, true);
						else add_nocopy(mcheckpowers, OPERATION_LOGICAL_AND, true);
						SWAP_CHILDREN(0, SIZE - 1)
						calculatesub(eo2, eo, false);
					}
					return true;
				} else if(CHILD(0).size() >= 2) {
					MathStructure mless1(CHILD(0)[0]);
					MathStructure mless2;
					if(CHILD(0).size() == 2) {
						mless2 = CHILD(0)[1];
					} else {
						mless2 = CHILD(0);
						mless2.delChild(1);
					}
					
					int checktype1 = 0, checktype2 = 0;
					MathStructure *mcheck1 = NULL, *mcheck2 = NULL;
					if(mless1.isPower() && !mless1[1].representsNonNegative()) {
						if(mless1[1].representsNegative()) {
							checktype1 = 1;
						} else if(ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER) {
							checktype1 = 2;
							mcheck1 = new MathStructure(mless1[1]);
							mcheck1->add(m_zero, OPERATION_EQUALS_GREATER);
							mcheck1->isolate_x_sub(eo, eo2, x_var);
							MathStructure *mcheck = new MathStructure(mless1[0]);
							mcheck->add(m_zero, OPERATION_NOT_EQUALS);
							mcheck1->isolate_x_sub(eo, eo2, x_var);
							mcheck1->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
							mcheck1->calculatesub(eo2, eo, false);
						}
					}
					if(mless2.isPower() && !mless2[1].representsNonNegative()) {
						if(mless2[1].representsNegative()) {
							checktype2 = 1;
						} else if(ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER) {
							checktype2 = 2;
							mcheck2 = new MathStructure(mless2[1]);
							mcheck2->add(m_zero, OPERATION_EQUALS_GREATER);
							mcheck2->isolate_x_sub(eo, eo2, x_var);
							MathStructure *mcheck = new MathStructure(mless2[0]);
							mcheck->add(m_zero, OPERATION_NOT_EQUALS);
							mcheck2->isolate_x_sub(eo, eo2, x_var);
							mcheck2->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
							mcheck2->calculatesub(eo2, eo, false);
						}
					}
					
					mless1.transform(STRUCT_COMPARISON, m_zero);
					if(checktype1 != 1 && (ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER)) {
						mless1.setComparisonType(COMPARISON_EQUALS_LESS);
					} else {
						mless1.setComparisonType(COMPARISON_LESS);
					}
					mless1.isolate_x_sub(eo, eo2, x_var);	
					mless2.transform(STRUCT_COMPARISON, m_zero);
					mless2.setComparisonType(COMPARISON_LESS);
					if(checktype2 != 1 && (ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER)) {
						mless2.setComparisonType(COMPARISON_EQUALS_LESS);
					} else {
						mless2.setComparisonType(COMPARISON_LESS);
					}
					mless2.isolate_x_sub(eo, eo2, x_var);
					MathStructure mgreater1(CHILD(0)[0]);
					mgreater1.transform(STRUCT_COMPARISON, m_zero);
					if(checktype1 != 1 && (ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER)) {
						mgreater1.setComparisonType(COMPARISON_EQUALS_GREATER);
					} else {
						mgreater1.setComparisonType(COMPARISON_GREATER);
					}
					mgreater1.isolate_x_sub(eo, eo2, x_var);
					MathStructure mgreater2;
					if(CHILD(0).size() == 2) {
						mgreater2 = CHILD(0)[1];
					} else {
						mgreater2 = CHILD(0);
						mgreater2.delChild(1);
					}
					mgreater2.transform(STRUCT_COMPARISON, m_zero);
					if(checktype2 != 1 && (ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_EQUALS_GREATER)) {
						mgreater2.setComparisonType(COMPARISON_EQUALS_GREATER);
					} else {
						mgreater2.setComparisonType(COMPARISON_GREATER);
					}
					mgreater2.isolate_x_sub(eo, eo2, x_var);
					clear();
					
					if(ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
						set(mless1);
						transform(STRUCT_LOGICAL_AND, mgreater2);
						calculatesub(eo2, eo, false);
						transform(STRUCT_LOGICAL_OR, mless2);
						CHILD(1).transform(STRUCT_LOGICAL_AND, mgreater1);
						CHILD(1).calculatesub(eo2, eo, false);
						CHILD_UPDATED(1)
						calculatesub(eo2, eo, false);
					} else {
						set(mless1);
						transform(STRUCT_LOGICAL_AND, mless2);
						calculatesub(eo2, eo, false);
						transform(STRUCT_LOGICAL_OR, mgreater1);
						CHILD(1).transform(STRUCT_LOGICAL_AND, mgreater2);
						CHILD(1).calculatesub(eo2, eo, false);
						CHILD_UPDATED(1)
						calculatesub(eo2, eo, false);
					}					
					if(checktype1 == 2) {
						add_nocopy(mcheck1, OPERATION_LOGICAL_AND, true);
						calculatesub(eo2, eo, false);
					}
					if(checktype2 == 2) {
						add_nocopy(mcheck2, OPERATION_LOGICAL_AND, true);
						calculatesub(eo2, eo, false);
					}
					return true;
					
				}
			}
			// x/(x+a)=b => x=b(x+a)
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(CALCULATOR->aborted()) return false;
				if(CHILD(0)[i].isPower() && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number().isNegative() && CHILD(0)[i][1].number().isReal()) {
					MathStructure *mtest;
					if(CHILD(0)[i][1].number().isMinusOne()) {
						CHILD(1).multiply(CHILD(0)[i][0]);
						mtest = new MathStructure(CHILD(0)[i][0]);
					} else {
						CHILD(0)[i][1].number().negate();
						CHILD(1).multiply(CHILD(0)[i]);
						mtest = new MathStructure(CHILD(0)[i]);
					}
					CHILD(0).delChild(i + 1);
					if(CHILD(0).size() == 1) CHILD(0).setToChild(1);
					CHILD(0).calculateSubtract(CHILD(1), eo);
					ComparisonType cmp_type = ct_comp;
					CHILD(1).clear();
					CHILDREN_UPDATED
					if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
						isolate_x_sub(eo, eo2, x_var, morig);
						mtest->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_EQUALS : OPERATION_NOT_EQUALS);
						mtest->isolate_x_sub(eo, eo2, x_var);
						add_nocopy(mtest, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
						calculatesub(eo2, eo, false);
						return true;
					} else {
						MathStructure *malt = new MathStructure(*this);
						if(ct_comp == COMPARISON_EQUALS_GREATER) {
							ct_comp = COMPARISON_EQUALS_LESS;
						} else if(ct_comp == COMPARISON_GREATER) {
							ct_comp = COMPARISON_LESS;
						} else if(ct_comp == COMPARISON_EQUALS_LESS) {
							ct_comp = COMPARISON_EQUALS_GREATER;
						} else if(ct_comp == COMPARISON_LESS) {
							ct_comp = COMPARISON_GREATER;
						}
						isolate_x_sub(eo, eo2, x_var, morig);
						mtest->add(m_zero, OPERATION_LESS);
						MathStructure *mtest_alt = new MathStructure(*mtest);
						mtest_alt->setComparisonType(COMPARISON_GREATER);
						mtest->isolate_x_sub(eo, eo2, x_var);
						add_nocopy(mtest, OPERATION_LOGICAL_AND);
						calculatesub(eo2, eo, false);
						malt->isolate_x_sub(eo, eo2, x_var, morig);
						mtest_alt->isolate_x_sub(eo, eo2, x_var);
						malt->add_nocopy(mtest_alt, OPERATION_LOGICAL_AND);
						malt->calculatesub(eo2, eo, false);
						add_nocopy(malt, OPERATION_LOGICAL_OR);
						calculatesub(eo2, eo, false);
						return true;
					}
				}
			}
			bool b = false;
			int zero1;
			if(CHILD(1).isZero()) zero1 = 1;
			else if(CHILD(1).representsNonZero(true)) zero1 = 0;
			else zero1 = 2;
			MathStructure *mcheckmulti = NULL, *mtryzero = NULL, *mchecknegative = NULL, *mchecknonzeropow = NULL;
			MathStructure mchild2(CHILD(1));
			MathStructure mbak(*this);
			ComparisonType ct_orig = ct_comp;
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(CALCULATOR->aborted()) {
					set(mbak); 
					if(mcheckmulti) mcheckmulti->unref();
					if(mtryzero) mcheckmulti->unref();
					if(mchecknegative) mcheckmulti->unref();
					if(mchecknonzeropow) mcheckmulti->unref();
					return false;
				}
				if(!CHILD(0)[i].contains(x_var)) {
					bool nonzero = false;
					if(CHILD(0)[i].isPower() && !CHILD(0)[i][1].representsNonNegative(true) && !CHILD(0)[i][0].representsNonZero(true)) {
						MathStructure *mcheck = new MathStructure(CHILD(0)[i][0]);
						if(ct_comp == COMPARISON_NOT_EQUALS) mcheck->add(m_zero, OPERATION_EQUALS);
						else mcheck->add(m_zero, OPERATION_NOT_EQUALS);
						mcheck->isolate_x(eo2, eo);
						if(CHILD(0)[i][1].representsNegative(true)) {
							nonzero = true;
						} else {
							MathStructure *mcheckor = new MathStructure(CHILD(0)[i][1]);
							if(ct_comp == COMPARISON_NOT_EQUALS) mcheckor->add(m_zero, OPERATION_EQUALS_LESS);
							else mcheckor->add(m_zero, OPERATION_EQUALS_GREATER);
							mcheckor->isolate_x(eo2, eo);
							if(ct_comp == COMPARISON_NOT_EQUALS) mcheck->add_nocopy(mcheckor, OPERATION_LOGICAL_AND);
							else mcheck->add_nocopy(mcheckor, OPERATION_LOGICAL_OR);
							mcheck->calculatesub(eo2, eo, false);
						}
						if(mchecknonzeropow) {
							if(ct_comp == COMPARISON_NOT_EQUALS) mchecknonzeropow->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
							else mchecknonzeropow->add_nocopy(mcheck, OPERATION_LOGICAL_AND);
						} else {
							mchecknonzeropow = mcheck;
						}
					}
					if(!nonzero && !CHILD(0)[i].representsNonZero(true)) {
						if(zero1 != 1 && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
							MathStructure *mcheck = new MathStructure(CHILD(0)[i]);
							if(ct_comp == COMPARISON_NOT_EQUALS) mcheck->add(m_zero, OPERATION_EQUALS);
							else mcheck->add(m_zero, OPERATION_NOT_EQUALS);
							mcheck->isolate_x(eo2, eo);
							if(mcheckmulti) {
								if(ct_comp == COMPARISON_NOT_EQUALS) mcheckmulti->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
								else mcheckmulti->add_nocopy(mcheck, OPERATION_LOGICAL_AND);
							} else {
								mcheckmulti = mcheck;	
							}
						}
						if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
							if(!mtryzero) {
								mtryzero = new MathStructure(CHILD(0)[i]);
								mtryzero->add(m_zero, OPERATION_EQUALS);
								mtryzero->isolate_x(eo2, eo);								
								MathStructure *mcheck = new MathStructure(mchild2);
								switch(ct_orig) {
									case COMPARISON_LESS: {mcheck->add(m_zero, OPERATION_GREATER); break;}
									case COMPARISON_GREATER: {mcheck->add(m_zero, OPERATION_LESS); break;}
									case COMPARISON_EQUALS_LESS: {mcheck->add(m_zero, OPERATION_EQUALS_GREATER); break;}
									case COMPARISON_EQUALS_GREATER: {mcheck->add(m_zero, OPERATION_EQUALS_LESS); break;}
									default: {}
								}
								mcheck->isolate_x(eo2, eo);
								mtryzero->add_nocopy(mcheck, OPERATION_LOGICAL_AND);		
							} else {
								MathStructure *mcheck = new MathStructure(CHILD(0)[i]);
								mcheck->add(m_zero, OPERATION_EQUALS);
								mcheck->isolate_x(eo2, eo);
								(*mtryzero)[0].add_nocopy(mcheck, OPERATION_LOGICAL_OR);
								mtryzero[0].calculateLogicalOrLast(eo2);
							}
						} else if(zero1 > 0) {
							MathStructure *mcheck = new MathStructure(CHILD(0)[i]);
							if(ct_comp == COMPARISON_NOT_EQUALS) mcheck->add(m_zero, OPERATION_NOT_EQUALS);
							else mcheck->add(m_zero, OPERATION_EQUALS);
							mcheck->isolate_x(eo2, eo);
							if(zero1 == 2) {
								MathStructure *mcheck2 = new MathStructure(mchild2);
								if(ct_comp == COMPARISON_NOT_EQUALS) mcheck2->add(m_zero, OPERATION_NOT_EQUALS);
								else mcheck2->add(m_zero, OPERATION_EQUALS);
								mcheck2->isolate_x(eo2, eo);
								if(ct_comp == COMPARISON_NOT_EQUALS) mcheck2->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
								else mcheck2->add_nocopy(mcheck, OPERATION_LOGICAL_AND);
								mcheck2->calculatesub(eo2, eo, false);
								mcheck = mcheck2;
							}							
							if(mtryzero) {
								if(ct_comp == COMPARISON_NOT_EQUALS) mtryzero->add_nocopy(mcheck, OPERATION_LOGICAL_AND);
								else mtryzero->add_nocopy(mcheck, OPERATION_LOGICAL_OR);
							} else {
								mtryzero = mcheck;
							}
						}
					}
					if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
						if(CHILD(0)[i].representsNegative()) {
							switch(ct_comp) {
								case COMPARISON_LESS: {ct_comp = COMPARISON_GREATER; break;}
								case COMPARISON_GREATER: {ct_comp = COMPARISON_LESS; break;}
								case COMPARISON_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
								case COMPARISON_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_LESS; break;}
								default: {}
							}
						} else if(!CHILD(0)[i].representsNonNegative()) {
							if(mchecknegative) {
								mchecknegative->multiply(CHILD(0)[i]);
							} else {
								mchecknegative = new MathStructure(CHILD(0)[i]);
							}
						}
					}
					if(zero1 != 1) {
						CHILD(1).calculateDivide(CHILD(0)[i], eo2);
						CHILD_UPDATED(1);
					}
					CHILD(0).delChild(i + 1);
					b = true;
				}
			}
			if(!b && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) && eo2.approximation != APPROXIMATION_EXACT && CHILD(1).isNumber() && CHILD(0).size() <= 3 && CHILD(0).size() >= 2) {
				//Check for x*e^x and solve with Lambert W function
				size_t e_index = CHILD(0).size();
				bool e_power = false;
				bool had_x_var = false;
				bool pow_num = false;
				bool multi_num = false;
				const MathStructure *x_struct = NULL;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CALCULATOR->aborted()) {
						set(mbak); 
						if(mcheckmulti) mcheckmulti->unref();
						if(mtryzero) mcheckmulti->unref();
						if(mchecknegative) mcheckmulti->unref();
						if(mchecknonzeropow) mcheckmulti->unref();
						return false;
					}
					if(x_struct && CHILD(0)[i] == *x_struct) {
						if(had_x_var) {
							had_x_var = false; 
							break;
						}
						had_x_var = true;
					} else if(CHILD(0)[i].isPower()) {
						if(!x_struct && CHILD(0)[i][0] == x_var) {
							x_struct = &CHILD(0)[i];
							if(had_x_var) {
								had_x_var = false;
								break;
							}
							had_x_var = true;
						} else {
							if(e_index < CHILD(0).size()) {
								e_index = CHILD(0).size();
								break;
							}
							if((CHILD(0)[i][0].isVariable() && CHILD(0)[i][0].variable() == CALCULATOR->v_e) || (CHILD(0)[i][0].isNumber() && CHILD(0)[i][0].number().isPositive())) {
								if(x_struct && CHILD(0)[i][1] == *x_struct) {
									e_index = i;
									e_power = CHILD(0)[i][0].isVariable();
								} else if(!x_struct && (CHILD(0)[i][1] == x_var || (CHILD(0)[i][1].isPower() && CHILD(0)[i][1][0] == x_var))) {
									x_struct = &CHILD(0)[i][1];
									e_index = i;
									e_power = CHILD(0)[i][0].isVariable();
								} else if(CHILD(0)[i][1].isMultiplication() && CHILD(0)[i][1].size() == 2 && CHILD(0)[i][1][0].isNumber() && !CHILD(0)[i][1][0].isZero()) {
									if(x_struct && CHILD(0)[i][1][1] == *x_struct) {
										e_index = i;
										e_power = CHILD(0)[i][0].isVariable();
										pow_num = true;
									} else if(!x_struct && (CHILD(0)[i][1][1] == x_var || (CHILD(0)[i][1][1].isPower() && CHILD(0)[i][1][1][0] == x_var))) {
										x_struct = &CHILD(0)[i][1][1];
										e_index = i;
										e_power = CHILD(0)[i][0].isVariable();
										pow_num = true;
									} else {
										break;
									}
								} else {
									break;
								}							
							} else {
								break;
							}
						}
					} else if(!x_struct && CHILD(0)[i] == x_var) {
						if(had_x_var) {
							had_x_var = false;
							break;
						}
						x_struct = &x_var;
						had_x_var = true;
					} else if(CHILD(0)[i].isNumber() && !CHILD(0)[i].isZero() && i == 0) {
						multi_num = true;
					} else {
						had_x_var = false;						
						break;
					}
				}				
				if(had_x_var && e_index < CHILD(0).size()) {
					Number num(CHILD(1).number());
					Number nr_pow(1, 1);
					if(pow_num) nr_pow = CHILD(0)[e_index][1][0].number();
					if(!e_power) {
						Number n_ln(CHILD(0)[e_index][0].number());
						n_ln.ln();
						nr_pow *= n_ln;						
						pow_num = true;						
					}
					if(multi_num) num /= CHILD(0)[0].number();
					if(pow_num) num *= nr_pow;					
					if(num.lambertW()) {
						CHILD(1) = num;
						if(e_power) {
							CHILD(0)[e_index].setToChild(2, true);
							CHILD(0).setToChild(e_index + 1, true);
						} else {
							MathStructure x_struct_new(*x_struct);
							CHILD(0).set(x_struct_new);
							CHILD(0).calculateMultiply(nr_pow, eo2);
						}						
						CHILDREN_UPDATED;
						isolate_x_sub(eo, eo2, x_var, morig);
						return true;
					}
				}
			}
			if(b) {
				if(CHILD(0).size() == 1) {
					CHILD(0).setToChild(1);
				}
				if((ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) && CHILD(1).contains(x_var)) {
					CHILD(0).calculateSubtract(CHILD(1), eo2);
					CHILD(1).clear();
				}
				if(mchecknegative) {
					MathStructure *mneg = new MathStructure(*this);
					switch(ct_comp) {
						case COMPARISON_LESS: {mneg->setComparisonType(COMPARISON_GREATER); break;}
						case COMPARISON_GREATER: {mneg->setComparisonType(COMPARISON_LESS); break;}
						case COMPARISON_EQUALS_LESS: {mneg->setComparisonType(COMPARISON_EQUALS_GREATER); break;}
						case COMPARISON_EQUALS_GREATER: {mneg->setComparisonType(COMPARISON_EQUALS_LESS); break;}
						default: {}
					}
					isolate_x_sub(eo, eo2, x_var, morig);
					//mneg->isolate_x_sub(eo, eo2, x_var);
					mchecknegative->add(m_zero, OPERATION_GREATER);
					add(*mchecknegative, OPERATION_LOGICAL_AND, true);
					SWAP_CHILDREN(0, SIZE - 1)
					calculatesub(eo2, eo, false);
					//LAST.isolate_x(eo2, eo);
					mchecknegative->setComparisonType(COMPARISON_LESS);
					mchecknegative->isolate_x(eo2, eo);
					mneg->add_nocopy(mchecknegative, OPERATION_LOGICAL_AND, true);
					mneg->calculatesub(eo2, eo, false);
					add_nocopy(mneg, OPERATION_LOGICAL_OR, true);
					calculatesub(eo2, eo, false);
				} else {
					isolate_x_sub(eo, eo2, x_var, morig);
				}
				if(mcheckmulti) {
					mcheckmulti->calculatesub(eo2, eo, false);
					if(ct_comp == COMPARISON_NOT_EQUALS) {
						add_nocopy(mcheckmulti, OPERATION_LOGICAL_OR, true);
					} else {
						add_nocopy(mcheckmulti, OPERATION_LOGICAL_AND, true);
						SWAP_CHILDREN(0, SIZE - 1)
					}
					calculatesub(eo2, eo, false);
				}
				if(mchecknonzeropow) {
					mchecknonzeropow->calculatesub(eo2, eo, false);
					if(ct_comp == COMPARISON_NOT_EQUALS) {
						add_nocopy(mchecknonzeropow, OPERATION_LOGICAL_OR, true);
					} else {
						add_nocopy(mchecknonzeropow, OPERATION_LOGICAL_AND, true);
						SWAP_CHILDREN(0, SIZE - 1)
					}
					calculatesub(eo2, eo, false);
				}
				if(mtryzero) {					
					mtryzero->calculatesub(eo2, eo, false);
					if(ct_comp == COMPARISON_NOT_EQUALS) {
						add_nocopy(mtryzero, OPERATION_LOGICAL_AND, true);
						SWAP_CHILDREN(0, SIZE - 1)
					} else {
						add_nocopy(mtryzero, OPERATION_LOGICAL_OR, true);
					}					
					calculatesub(eo2, eo, false);
				}
				return true;
			}
			
			if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
				// x*(x+a)^(1/b)=c => (x*(x+a)^(1/b))^b=c^b
				Number nrexp;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CALCULATOR->aborted()) return false;
					if(CHILD(0)[i].isPower() && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number().isRational()) {
						if(!CHILD(0)[i][1].number().isInteger()) {
							if(nrexp.isZero()) nrexp = CHILD(0)[i][1].number().denominator();
							else nrexp.lcm(CHILD(0)[i][1].number().denominator());
						}
					} else if(CHILD(0)[i].isFunction() && CHILD(0)[i].function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0)[i])) {
						if(nrexp.isZero()) nrexp = CHILD(0)[i][1].number();
						else nrexp.lcm(CHILD(0)[i][1].number());
					} else if(CHILD(0)[i] != x_var && !(CHILD(0)[i].isFunction() && CHILD(0)[i].function() == CALCULATOR->f_abs && CHILD(0)[i].size() == 1 && CHILD(0)[i][0].representsReal(true))) {
						nrexp.clear();
						break;
					}
				}
				if(!nrexp.isZero()) {
					MathStructure mtest(*this);
					if(mtest[0].calculateRaise(nrexp, eo2)) {
						mtest[1].calculateRaise(nrexp, eo2);
						mtest.childrenUpdated();
						if(mtest.isolate_x(eo2, eo, x_var)) {
							if(test_comparisons(*this, mtest, x_var, eo) >= 0) {
								set(mtest);
								return true;
							}
						}
					}
				}
			}
			if(!eo2.expand) break;
			// abs(x)*x=a => -x*x=a || x*x=a; sgn(x)*x=a => -1*x=a || 0*x=a || 1*x=a
			if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
				MathStructure *m = find_abs_sgn(CHILD(0), x_var);
				if(m && m->function() == CALCULATOR->f_abs) {

					MathStructure mabs(*m);

					ComparisonType cmp_type = ct_comp;
					MathStructure *malt = new MathStructure(*this);
					MathStructure mabs_minus(mabs[0]);
					mabs_minus.calculateNegate(eo2);
					(*malt)[0].replace(mabs, mabs_minus, mabs.containsInterval());
					(*malt)[0].calculatesub(eo2, eo, true);
					MathStructure *mcheck = new MathStructure(mabs[0]);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_EQUALS_GREATER : OPERATION_LESS);
					mcheck->isolate_x_sub(eo, eo2, x_var);
					malt->isolate_x_sub(eo, eo2, x_var);
					malt->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					malt->calculatesub(eo2, eo, false);
					
					mcheck = new MathStructure(mabs[0]);
					CHILD(0).replace(mabs, mabs[0], mabs.containsInterval());
					CHILD(0).calculatesub(eo2, eo, true);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LESS : OPERATION_EQUALS_GREATER);
					mcheck->isolate_x_sub(eo, eo2, x_var);
					isolate_x_sub(eo, eo2, x_var);
					add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					calculatesub(eo2, eo, false);
					add_nocopy(malt, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					calculatesub(eo2, eo, false);
					return true;
				} else if(m && m->function() == CALCULATOR->f_signum) {
					MathStructure mabs(*m);

					ComparisonType cmp_type = ct_comp;
					MathStructure *malt = new MathStructure(*this);
					(*malt)[0].replace(mabs, m_minus_one, mabs.containsInterval());
					(*malt)[0].calculatesub(eo2, eo, true);
					MathStructure *mcheck = new MathStructure(mabs[0]);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_EQUALS_GREATER : OPERATION_LESS);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? ((*m)[1].isMinusOne() ? OPERATION_GREATER : OPERATION_EQUALS_GREATER) : ((*m)[1].isMinusOne() ? OPERATION_EQUALS_LESS : OPERATION_LESS));
					mcheck->isolate_x_sub(eo, eo2, x_var);
					malt->isolate_x_sub(eo, eo2, x_var);
					malt->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					malt->calculatesub(eo2, eo, false);
					
					MathStructure *malt0 = NULL;
					if(!(*m)[1].isOne() && !(*m)[1].isMinusOne()) {
						malt0 = new MathStructure(*this);
						(*malt0)[0].replace(mabs, (*m)[1], mabs.containsInterval());
						(*malt0)[0].calculatesub(eo2, eo, true);
						mcheck = new MathStructure(mabs[0]);
						mcheck->add((*m)[1], cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_NOT_EQUALS : OPERATION_EQUALS);
						mcheck->isolate_x_sub(eo, eo2, x_var);
						malt0->isolate_x_sub(eo, eo2, x_var);
						malt0->add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
						malt0->calculatesub(eo2, eo, false);
					}
					
					mcheck = new MathStructure(mabs[0]);
					CHILD(0).replace(mabs, m_one, mabs.containsInterval());
					CHILD(0).calculatesub(eo2, eo, true);
					mcheck->add(m_zero, cmp_type == COMPARISON_NOT_EQUALS ? ((*m)[1].isOne() ? OPERATION_LESS : OPERATION_EQUALS_LESS) : ((*m)[1].isOne() ? OPERATION_EQUALS_GREATER : OPERATION_GREATER));
					mcheck->isolate_x_sub(eo, eo2, x_var);
					isolate_x_sub(eo, eo2, x_var);
					add_nocopy(mcheck, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND, true);
					calculatesub(eo2, eo, false);
					add_nocopy(malt, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					if(malt0) add_nocopy(malt0, cmp_type == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
					calculatesub(eo2, eo, false);
					return true;
				}
			}
			// Try factorization
			if(!CHILD(1).isZero() && (!morig || !equals(*morig))) {
				MathStructure mtest(*this);
				mtest[0].calculateSubtract(CHILD(1), eo2);
				mtest[1].clear();
				mtest.childrenUpdated();
				if(mtest[0].factorize(eo2, false, false, 0, false, false) && !(mtest[0].isMultiplication() && mtest[0].size() == 2 && (mtest[0][0].isNumber() || mtest[0][0] == CHILD(1) || mtest[0][1] == CHILD(1)))) {
					mtest.childUpdated(1);
					if(mtest.isolate_x_sub(eo, eo2, x_var, this)) {
						set_nocopy(mtest);
						return true;
					}
				}
			}
			break;
		} 
		case STRUCT_POWER: {
			if(CHILD(0)[0].contains(x_var)) {
				if(ct_comp == COMPARISON_EQUALS && eo2.approximation != APPROXIMATION_EXACT && CHILD(1).isNumber() && CHILD(1).number().isPositive() && CHILD(0)[1] == x_var && CHILD(0)[0] == x_var && CHILD(0)[0].representsReal(true)) {
					// x^x=a => x=ln(a)/lambertw(ln(a))
					if(CHILD(1).number().isInterval() && CHILD(1).number().isPositive()) {
						Number nrlow(CHILD(1).number().lowerEndPoint());
						Number nrhigh(CHILD(1).number().upperEndPoint());
						if(solve_x_pow_x(nrlow) && solve_x_pow_x(nrhigh)) {
							if(!CHILD(1).number().setInterval(nrlow, nrhigh, true)) return false;
							CHILD(0) = x_var;
							CHILDREN_UPDATED
							return true;
						}
					} else {
						if(solve_x_pow_x(CHILD(1).number())) {
							CHILD(0) = x_var;
							CHILDREN_UPDATED
							return true;
						}
					}
				} else if(CHILD(0)[1].isNumber() && CHILD(0)[1].number().isRational()) {
					// x^a=b
					bool b_neg = CHILD(0)[1].number().isNegative();
					bool b_nonzero = !CHILD(1).isZero() && CHILD(1).representsNonZero(true);
					if(b_neg && CHILD(1).isZero()) {
						if(ct_comp == COMPARISON_EQUALS) {
							clear(true);
							return true;
						} else if(ct_comp == COMPARISON_NOT_EQUALS) {
							set(1, 1, 0, true);
							return true;
						}
						if(CHILD(0)[1].number().isInteger() && CHILD(0)[1].number().isEven()) {
							if(ct_comp == COMPARISON_EQUALS_LESS) {
								clear(true);
								return true;
							}
							ct_comp = COMPARISON_NOT_EQUALS;
							CHILD(1).clear(true);
							CHILD(0).setToChild(1);
							CHILDREN_UPDATED
							isolate_x_sub(eo, eo2, x_var, morig);
							return true;
						}
						if(ct_comp == COMPARISON_EQUALS_GREATER) {
							ct_comp = COMPARISON_GREATER;
						} else if(ct_comp == COMPARISON_EQUALS_LESS) {
							ct_comp = COMPARISON_LESS;
						}
						CHILD(1).clear(true);
						CHILD(0).setToChild(1);
						CHILDREN_UPDATED
						isolate_x_sub(eo, eo2, x_var, morig);
						return true;
					} else if(CHILD(1).isZero()) {
						if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS && CHILD(0)[1].number().isInteger() && CHILD(0)[1].number().isEven()) {
							if(ct_comp == COMPARISON_LESS) {
								clear(true);
								return true;
							}
							if(ct_comp == COMPARISON_EQUALS_LESS) {
								ct_comp = COMPARISON_EQUALS;
								CHILD(0).setToChild(1);
								CHILDREN_UPDATED
								isolate_x_sub(eo, eo2, x_var, morig);
								return true;
							}
							if(ct_comp == COMPARISON_EQUALS_GREATER) {
								set(1, 1, 0, true);
								return true;
							}
							CHILD(0).setToChild(1);
							MathStructure *mneg = new MathStructure(*this);
							if(ct_comp == COMPARISON_GREATER) mneg->setComparisonType(COMPARISON_LESS);
							mneg->isolate_x_sub(eo, eo2, x_var, morig);
							add_nocopy(mneg, OPERATION_LOGICAL_OR);
							calculatesub(eo2, eo, false);
						} else {
							CHILD(0).setToChild(1);
							CHILDREN_UPDATED
							isolate_x_sub(eo, eo2, x_var, morig);
						}
						return true;
					} else if(b_neg && ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
						if(CHILD(0)[1].number().isMinusOne()) {
							CHILD(0).setToChild(1);
						} else {
							CHILD(0)[1].number().negate();
							
						}
						MathStructure *mtest = new MathStructure(CHILD(0));
						CHILD(1).set(1, 1, 0);
						CHILDREN_UPDATED
						MathStructure *malt = new MathStructure(*this);
						if(ct_comp == COMPARISON_EQUALS_GREATER) {
							ct_comp = COMPARISON_EQUALS_LESS;
						} else if(ct_comp == COMPARISON_GREATER) {
							ct_comp = COMPARISON_LESS;
						} else if(ct_comp == COMPARISON_EQUALS_LESS) {
							ct_comp = COMPARISON_EQUALS_GREATER;
						} else if(ct_comp == COMPARISON_LESS) {
							ct_comp = COMPARISON_GREATER;
						}
						isolate_x_sub(eo, eo2, x_var, morig);
						mtest->add(m_zero, OPERATION_GREATER);
						MathStructure *mtest_alt = new MathStructure(*mtest);
						mtest_alt->setComparisonType(COMPARISON_LESS);
						mtest->isolate_x_sub(eo, eo2, x_var);
						add_nocopy(mtest, OPERATION_LOGICAL_AND);
						calculatesub(eo2, eo, false);
						malt->isolate_x_sub(eo, eo2, x_var, morig);
						mtest_alt->isolate_x_sub(eo, eo2, x_var);
						malt->add_nocopy(mtest_alt, OPERATION_LOGICAL_AND);
						malt->calculatesub(eo2, eo, false);
						add_nocopy(malt, OPERATION_LOGICAL_OR);
						calculatesub(eo2, eo, false);
						return true;
					}
					MathStructure mbak(*this);
					if(CHILD(0)[1].number().isMinusOne()) {
						CHILD(0).setToChild(1, true);
						CHILD(1).calculateRaise(m_minus_one, eo2);
						CHILDREN_UPDATED
						isolate_x_sub(eo, eo2, x_var, morig);
					} else if(CHILD(0)[1].number().isInteger()) {
						bool b_real = CHILD(0)[0].representsReal(true);
						bool b_complex = !b_real && CHILD(0)[0].representsComplex(true);
						bool warn_complex = false;
						bool check_complex = false;
						if(CHILD(0)[1].number().isEven()) {
							if(!CHILD(1).representsNonNegative(true)) {
								if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
									if(CHILD(1).representsNegative(true)) {
										if(ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_LESS) clear(true);
										else set(1, 1, 0, true);
										return true;
									}
									return false;
								}
								if(b_real && (CHILD(1).representsNegative(true) || CHILD(1).representsComplex(true))) {
									if(ct_comp == COMPARISON_EQUALS) {
										clear(true);
									} else if(ct_comp == COMPARISON_NOT_EQUALS) {
										set(1, 1, 0, true);
									}
									return true;
								}
							}
						}
						bool b_set = false;
						if(b_neg) CHILD(0)[1].number().negate();
						if(CHILD(0)[1].number().isTwo()) {
							CHILD(1).raise(CHILD(0)[1].number());
							CHILD(1)[1].number().recip();
							if(b_neg) CHILD(1)[1].number().negate();
							CHILD(1).calculateRaiseExponent(eo2);
							CHILDREN_UPDATED
						} else if(!b_real && CHILD(0)[1].number() == 4 && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
							CHILD(1).raise(CHILD(0)[1].number());
							CHILD(1)[1].number().recip();
							if(b_neg) CHILD(1)[1].number().negate();
							CHILD(1).calculateRaiseExponent(eo2);
							CHILD(0).setToChild(1);
							CHILDREN_UPDATED
							MathStructure *malt1 = new MathStructure(*this);
							MathStructure *malt2 = new MathStructure(*this);
							MathStructure *malt3 = new MathStructure(*this);
							(*malt1)[1].calculateNegate(eo2);
							(*malt2)[1].calculateMultiply(m_one_i, eo2);
							(*malt3)[1].calculateMultiply(nr_minus_i, eo2);
							malt1->childUpdated(2);
							malt2->childUpdated(2);
							malt3->childUpdated(2);
							malt1->isolate_x_sub(eo, eo2, x_var, morig);
							malt2->isolate_x_sub(eo, eo2, x_var, morig);
							malt3->isolate_x_sub(eo, eo2, x_var, morig);
							isolate_x_sub(eo, eo2, x_var, morig);
							add_nocopy(malt1, ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR);
							add_nocopy(malt2, ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
							add_nocopy(malt3, ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
							calculatesub(eo2, eo, false);
							b_set = true;
						} else if(!b_real && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
							MathStructure mdeg(CHILD(0)[1]);
							Number marg_pi;
							MathStructure marg;
							if(CHILD(1).representsNegative(true)) {
								marg_pi.set(1, 1, 0);
							} else if(!CHILD(1).representsNegative(true)) {
								if(CHILD(1).isNumber() && !CHILD(1).number().hasRealPart()) {
									if(CHILD(1).number().imaginaryPartIsNegative()) marg_pi.set(-1, 2, 0);
									else marg_pi.set(1, 2, 0);
								} else {
									marg.set(CALCULATOR->f_arg, &CHILD(1), NULL);
									marg.calculateFunctions(eo);
									switch(eo2.parse_options.angle_unit) {
										case ANGLE_UNIT_DEGREES: {marg.multiply(Number(1, 180, 0)); marg.multiply(CALCULATOR->v_pi); break;}
										case ANGLE_UNIT_GRADIANS: {marg.multiply(Number(1, 200, 0)); marg.multiply(CALCULATOR->v_pi); break;}
										case ANGLE_UNIT_RADIANS: {break;}
										default: {if(CALCULATOR->getRadUnit()) marg /= CALCULATOR->getRadUnit();}
									}
									marg.calculatesub(eo2, eo, true);
								}
							}
							MathStructure minv(mdeg);
							minv.number().recip();
							MathStructure mmul(CALCULATOR->f_abs, &CHILD(1), NULL);
							mmul.calculateFunctions(eo);
							mmul.calculateRaise(minv, eo2);
							Number nr_i;
							while(nr_i.isLessThan(mdeg.number())) {
								MathStructure mroot;
								if(CALCULATOR->aborted()) {set(mbak); return false;}
								MathStructure mexp;
								Number nexp;
								if(!nr_i.isZero()) {
									nexp.set(2, 1, 0);
									if(!nexp.multiply(nr_i)) {set(mbak); return false;}
								}
								b_set = false;
								if(!marg_pi.isZero()) {
									if(nexp.isZero()) nexp = marg_pi;
									else if(!nexp.add(marg_pi)) {set(mbak); return false;}
									if(!nexp.divide(mdeg.number())) {set(mbak); return false;}
									if(nexp.isInteger()) {
										mroot.set(mmul);
										if(nexp.isOdd()) {
											mroot.calculateNegate(eo2);
										}
										b_set = true;
									} else if(nexp.isRational() && nexp.denominatorIsTwo()) {
										if(!nexp.floor()) {set(mbak); return false;}
										mroot.set(mmul);
										if(nexp.isEven()) {
											mroot.calculateMultiply(nr_one_i, eo2);
										} else {
											mroot.calculateMultiply(nr_minus_i, eo2);
										}
										b_set = true;
									}
									if(!b_set) {
										mexp.set(nexp);
										mexp.multiply(CALCULATOR->v_pi);
									}
								} else {
									if(nexp.isZero()) {
										if(!marg.isZero()) {
											mexp.set(marg);
											mexp.multiply(minv);
										}
									} else {
										mexp.set(nexp);
										mexp.multiply(CALCULATOR->v_pi);
										if(!marg.isZero()) mexp.add(marg);
										mexp.multiply(minv);
									}
								}
								if(!b_set) {
									if(mexp.isZero()) {
										mroot.set(mmul);
									} else {
										mexp.multiply(m_one_i);
										mroot.set(CALCULATOR->v_e);
										mroot.raise(mexp);
										mroot.calculatesub(eo2, eo, true);
										mroot.calculateMultiply(mmul, eo2);
									}
								}
								if(b_neg) mroot.calculateRaise(m_minus_one, eo2);
								if(nr_i.isZero()) {
									CHILD(0).setToChild(1);
									CHILD(1) = mroot;
									isolate_x_sub(eo, eo2, x_var, morig);
								} else {
									MathStructure *malt = new MathStructure(mbak[0][0]);
									malt->add(mroot, mbak.comparisonType() == COMPARISON_NOT_EQUALS ? OPERATION_NOT_EQUALS : OPERATION_EQUALS);
									malt->isolate_x_sub(eo, eo2, x_var, morig);
									add_nocopy(malt, mbak.comparisonType() == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR, true);
								}
								nr_i++;
							}
							calculatesub(eo2, eo, false);
							b_set = true;
						} else {
							if(b_complex) {
								warn_complex = true;
							} else if(!b_real) {
								check_complex = true;
							}
							CHILD(1).transform(STRUCT_FUNCTION, CHILD(0)[1]);
							CHILD(1).setFunction(CALCULATOR->f_root);
							CHILD(1).calculateFunctions(eo);
							if(b_neg) CHILD(1).calculateRaise(m_minus_one, eo2);
							childUpdated(2);
						}
						if(!b_set) {
							if(CHILD(0)[1].number().isEven()) {
								CHILD(0).setToChild(1);
								MathStructure *mneg = new MathStructure(*this);
								(*mneg)[1].calculateNegate(eo2);
								mneg->childUpdated(2);
								if(ct_comp == COMPARISON_LESS) mneg->setComparisonType(COMPARISON_GREATER);
								else if(ct_comp == COMPARISON_GREATER) mneg->setComparisonType(COMPARISON_LESS);
								else if(ct_comp == COMPARISON_EQUALS_LESS) mneg->setComparisonType(COMPARISON_EQUALS_GREATER);
								else if(ct_comp == COMPARISON_EQUALS_GREATER) mneg->setComparisonType(COMPARISON_EQUALS_LESS);
								mneg->isolate_x_sub(eo, eo2, x_var, morig);
								isolate_x_sub(eo, eo2, x_var, morig);
								add_nocopy(mneg, ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS ? OPERATION_LOGICAL_AND : OPERATION_LOGICAL_OR);
								calculatesub(eo2, eo, false);
							} else {
								CHILD(0).setToChild(1);
								isolate_x_sub(eo, eo2, x_var, morig);
							}
						}
						if(check_complex) {
							if(!isComparison() || CHILD(0) != x_var) {
								warn_complex = true;
							}
							MathStructure mtest(mbak[0][0]);
							if(!warn_complex && check_complex) {
								mtest.replace(x_var, CHILD(1));
								if(mtest.representsReal(true)) check_complex = false;
								if(mtest.representsComplex(true)) {
									warn_complex = true;
								}
							}
							if(!warn_complex && check_complex) {
								CALCULATOR->beginTemporaryStopMessages();
								EvaluationOptions eo3 = eo;
								eo3.approximation = APPROXIMATION_APPROXIMATE;
								mtest.eval(eo3);
								if(CALCULATOR->endTemporaryStopMessages() || !mtest.representsReal(true)) {
									warn_complex = true;
								}
							}
						}
						if(warn_complex) CALCULATOR->error(false, _("Only one or two of the roots where calculated for %s."), mbak.print().c_str(), NULL);
					} else {
						MathStructure *mposcheck = NULL;
						bool b_test = false;
						if(!CHILD(1).representsNonNegative(true)) {
							if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
								if(CHILD(1).representsNegative(true)) {
									if(ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_LESS) clear(true);
									else set(1, 1, 0, true);
									return true;
								}
								return false;
							}
							if(CHILD(1).representsNegative(true)) {
								if(ct_comp == COMPARISON_EQUALS) {
									clear(true);
								} else if(ct_comp == COMPARISON_NOT_EQUALS) {
									set(1, 1, 0, true);
								}
								return true;
							}
							if(CHILD(1).representsReal(true)) {
								mposcheck = new MathStructure(CHILD(1));
								mposcheck->add(m_zero, ct_comp == COMPARISON_NOT_EQUALS ? OPERATION_LESS : OPERATION_EQUALS_GREATER);
								mposcheck->isolate_x_sub(eo, eo2, x_var);
							} else {
								b_test = true;
								mposcheck = new MathStructure(*this);
							}
						}
						CHILD(0)[1].number().recip();
						CHILD(1).calculateRaise(CHILD(0)[1], eo);
						CHILD(0).setToChild(1);
						CHILDREN_UPDATED
						isolate_x_sub(eo, eo2, x_var, morig);
						if(b_test) {
							if(test_comparisons(*mposcheck, *this, x_var, eo) < 0) {
								mposcheck->unref();
								return false;
							}
							mposcheck->unref();
						} else if(mposcheck) {
							add_nocopy(mposcheck, ct_comp == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND);
							calculatesub(eo2, eo, false);
						}
					}
					if(b_neg && !b_nonzero) {
						MathStructure *mtest = new MathStructure(mbak[1]);
						mtest->add(m_zero, (mbak.comparisonType() == COMPARISON_NOT_EQUALS) ? OPERATION_EQUALS : OPERATION_NOT_EQUALS);
						mtest->isolate_x_sub(eo, eo2, x_var);
						add_nocopy(mtest, (mbak.comparisonType() == COMPARISON_NOT_EQUALS) ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND);
						calculatesub(eo2, eo, false);
						if(mbak.comparisonType() != COMPARISON_NOT_EQUALS && mbak.comparisonType() != COMPARISON_EQUALS) {
							MathStructure *malt = new MathStructure(mbak[0]);
							if(mbak[0][1].representsInteger() && mbak[0][1].representsEven()) {
								malt->add(m_zero, OPERATION_NOT_EQUALS);
							} else {
								malt->add(m_zero, (ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_GREATER) ? OPERATION_LESS : OPERATION_GREATER);
							}
							malt->isolate_x_sub(eo, eo2, x_var, morig);
							MathStructure *mtest2 = new MathStructure(mbak[1]);
							mtest2->add(m_zero, OPERATION_EQUALS);
							mtest2->isolate_x_sub(eo, eo2, x_var);
							malt->add_nocopy(mtest2, (mbak.comparisonType() == COMPARISON_NOT_EQUALS) ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND);
							malt->calculatesub(eo2, eo, false);
							add_nocopy(malt, OPERATION_LOGICAL_OR);
							calculatesub(eo2, eo, false);
						}
					}
					return true;
				}
			} else if(CHILD(0)[1].contains(x_var) && CHILD(0)[0].representsNonNegative(true)) {
				// a^x=b => x=log(b, a)
				if(CHILD(1).isOne()) {
					if(!CHILD(0)[0].representsUndefined(true, true)) {
						CHILD(0).setToChild(2, true);
						CHILD(1).clear(true);
						isolate_x_sub(eo, eo2, x_var, morig);
						return true;
					}
				}
				if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
					if(CHILD(0)[0].isNumber() && CHILD(0)[0].number().isReal() && CHILD(0)[0].number().isPositive()) {
						if(CHILD(1).representsNegative()) {
							if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) {
								set(1, 1, 0, true);
							} else {
								clear(true);
							}
							return true;
						}
						if(CHILD(0)[0].number().isFraction()) {
							switch(ct_comp) {
								case COMPARISON_LESS: {ct_comp = COMPARISON_GREATER; break;}
								case COMPARISON_GREATER: {ct_comp = COMPARISON_LESS; break;}
								case COMPARISON_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
								case COMPARISON_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_LESS; break;}
								default: {}
							}
						}
					} else {
						return false;
					}
				}
				MathStructure msave(CHILD(1));
				CHILD(1).set(CALCULATOR->f_logn, &msave, &CHILD(0)[0], NULL);
				bool b = CHILD(1).calculateFunctions(eo);
				CHILD(1).unformat(eo);
				if(b) CHILD(1).calculatesub(eo2, eo, true);
				CHILD(0).setToChild(2, true);
				CHILDREN_UPDATED;
				isolate_x_sub(eo, eo2, x_var, morig);
				return true;
			}
			break;
		}
		case STRUCT_FUNCTION: {
			if(CHILD(0).function() == CALCULATOR->f_root && VALID_ROOT(CHILD(0))) {
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure *mposcheck = NULL;
					bool b_test = false;
					if(CHILD(0)[1].number().isEven() && !CHILD(1).representsNonNegative(true)) {
						if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
							if(CHILD(1).representsNegative(true)) {
								if(ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_LESS) clear(true);
								else set(1, 1, 0, true);
								return true;
							}
							return false;
						}
						if(CHILD(1).representsNegative(true)) {
							if(ct_comp == COMPARISON_EQUALS) {
								clear(true);
							} else if(ct_comp == COMPARISON_NOT_EQUALS) {
								set(1, 1, 0, true);
							}
							return true;
						}
						if(CHILD(1).representsReal(true)) {
							mposcheck = new MathStructure(CHILD(1));
							mposcheck->add(m_zero, ct_comp == COMPARISON_NOT_EQUALS ? OPERATION_LESS : OPERATION_EQUALS_GREATER);
							mposcheck->isolate_x_sub(eo, eo2, x_var);
						} else {
							b_test = true;
							mposcheck = new MathStructure(*this);
						}
					}
					CHILD(1).calculateRaise(CHILD(0)[1], eo);
					CHILD(0).setToChild(1);
					CHILDREN_UPDATED
					isolate_x_sub(eo, eo2, x_var, morig);
					if(b_test) {
						if(test_comparisons(*mposcheck, *this, x_var, eo) < 0) {
							mposcheck->unref();
							return false;
						}
						mposcheck->unref();
					} else if(mposcheck) {
						add_nocopy(mposcheck, ct_comp == COMPARISON_NOT_EQUALS ? OPERATION_LOGICAL_OR : OPERATION_LOGICAL_AND);
						calculatesub(eo2, eo, false);
					}
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_ln && CHILD(0).size() == 1) {
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1).set(CALCULATOR->v_e);
					CHILD(1).calculateRaise(msave, eo2);
					CHILD(0).setToChild(1, true);
					CHILDREN_UPDATED;
					if(ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
						MathStructure *mand = new MathStructure(CHILD(0));
						mand->add(m_zero, OPERATION_GREATER);
						mand->isolate_x_sub(eo, eo2, x_var);
						isolate_x_sub(eo, eo2, x_var, morig);
						add_nocopy(mand, OPERATION_LOGICAL_AND);
						SWAP_CHILDREN(0, 1);
						calculatesub(eo2, eo, false);
					} else {
						isolate_x_sub(eo, eo2, x_var, morig);
					}
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_lambert_w && CHILD(0).size() == 1 && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1).set(CALCULATOR->v_e);
					CHILD(1).calculateRaise(msave, eo2);
					CHILD(1).calculateMultiply(msave, eo2);
					CHILD(0).setToChild(1, true);
					CHILDREN_UPDATED;
					if(ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
						MathStructure *mand = new MathStructure(CHILD(0));
						mand->add(m_zero, OPERATION_GREATER);
						mand->isolate_x_sub(eo, eo2, x_var);
						isolate_x_sub(eo, eo2, x_var, morig);
						add_nocopy(mand, OPERATION_LOGICAL_AND);
						SWAP_CHILDREN(0, 1);
						calculatesub(eo2, eo, false);
					} else {
						isolate_x_sub(eo, eo2, x_var, morig);
					}
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_logn && CHILD(0).size() == 2) {
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1) = CHILD(0)[1];
					CHILD(1).calculateRaise(msave, eo2);
					CHILD(0).setToChild(1, true);
					CHILDREN_UPDATED;
					if(ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS) {
						MathStructure *mand = new MathStructure(CHILD(0));
						mand->add(m_zero, OPERATION_GREATER);
						mand->isolate_x_sub(eo, eo2, x_var);
						isolate_x_sub(eo, eo2, x_var, morig);
						add_nocopy(mand, OPERATION_LOGICAL_AND);
						SWAP_CHILDREN(0, 1);
						calculatesub(eo2, eo, false);
					} else {
						isolate_x_sub(eo, eo2, x_var, morig);
					}
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_abs && CHILD(0).size() == 1) {
				if(CHILD(0)[0].contains(x_var)) {
					bool b_and = ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS || ct_comp == COMPARISON_EQUALS_LESS;
					CHILD(0).setToChild(1);
					CHILD_UPDATED(0)					
					MathStructure *mchild2 = new MathStructure(*this);
					isolate_x_sub(eo, eo2, x_var, morig);
					(*mchild2)[0].calculateNegate(eo2);
					mchild2->childUpdated(1);
					mchild2->isolate_x_sub(eo, eo2, x_var);
					if(b_and) {
						add_nocopy(mchild2, OPERATION_LOGICAL_AND);
					} else {
						add_nocopy(mchild2, OPERATION_LOGICAL_OR);
					}
					calculatesub(eo2, eo, false);
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_signum && CHILD(0).size() == 2) {
				if(CHILD(0)[0].contains(x_var) && CHILD(0)[0].representsReal(true)) {
					if(CHILD(1).isZero() && !CHILD(0)[1].isOne() && !CHILD(0)[1].isMinusOne()) {
						CHILD(0).setToChild(2, true, this, 1);
						isolate_x_sub(eo, eo2, x_var, morig);
						return true;
					}
					if(CHILD(1).isNumber() && !CHILD(1).number().isInterval()) {
						if(CHILD(1).number().isOne()) {
							if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_EQUALS_GREATER) ct_comp = (CHILD(0)[1].isOne() ? COMPARISON_EQUALS_GREATER : COMPARISON_GREATER);
							else if(ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_LESS) ct_comp = (CHILD(0)[1].isOne() ? COMPARISON_LESS : COMPARISON_EQUALS_LESS);
							else if(ct_comp == COMPARISON_GREATER) {clear(true); return 1;}
							else {set(1, 1, 0, true); return 1;}
							CHILD(0).setToChild(1, true, this, 1);
							CHILD(1).clear(true);
							isolate_x_sub(eo, eo2, x_var, morig);
							return true;
						} else if(CHILD(1).number().isMinusOne()) {
							if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_EQUALS_LESS) ct_comp = (CHILD(0)[1].isMinusOne() ?COMPARISON_EQUALS_LESS : COMPARISON_LESS);
							else if(ct_comp == COMPARISON_NOT_EQUALS || ct_comp == COMPARISON_GREATER) ct_comp = (CHILD(0)[1].isMinusOne() ? COMPARISON_GREATER : COMPARISON_EQUALS_GREATER);
							else if(ct_comp == COMPARISON_LESS) {clear(true); return 1;}
							else {set(1, 1, 0, true); return 1;}
							CHILD(0).setToChild(1, true, this, 1);
							CHILD(1).clear(true);
							isolate_x_sub(eo, eo2, x_var, morig);
							return true;
						}
						if(ct_comp == COMPARISON_EQUALS) {
							clear(true);
							return true;
						} else if(ct_comp == COMPARISON_NOT_EQUALS) {
							set(1, 1, 0, true);
							return true;
						}
						if(CHILD(0)[1].isZero() || CHILD(0)[1].isOne() || CHILD(0)[1].isMinusOne()) {
							if(CHILD(1).number().isPositive()) {
								if(CHILD(1).number().isGreaterThan(1)) {
									if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) clear(true);
									else set(1, 1, 0, true);
									return 1;
								}
								if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) ct_comp = (CHILD(0)[1].isOne() ? COMPARISON_EQUALS_GREATER : COMPARISON_GREATER);
								else ct_comp = (CHILD(0)[1].isOne() ? COMPARISON_LESS : COMPARISON_EQUALS_LESS);
								CHILD(0).setToChild(1, true, this, 1);
								CHILD(1).clear(true);
								isolate_x_sub(eo, eo2, x_var, morig);
								return true;
							} else if(CHILD(1).number().isNegative()) {
								if(CHILD(1).number().isLessThan(-1)) {
									if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) set(1, 1, 0, true);
									else clear(true);
									return 1;
								}
								if(ct_comp == COMPARISON_GREATER || ct_comp == COMPARISON_EQUALS_GREATER) ct_comp = (CHILD(0)[1].isMinusOne() ? COMPARISON_GREATER : COMPARISON_EQUALS_GREATER);
								else ct_comp = (CHILD(0)[1].isMinusOne() ? COMPARISON_EQUALS_LESS : COMPARISON_LESS);
								CHILD(0).setToChild(1, true, this, 1);
								CHILD(1).clear(true);
								isolate_x_sub(eo, eo2, x_var, morig);
								return true;
							}
						}
					}
				}
			}
			break;
		}
		default: {}
	}
	return false;
}

bool MathStructure::isolate_x(const EvaluationOptions &eo, const MathStructure &x_varp, bool check_result) {
	return isolate_x(eo, eo, x_varp, check_result);
}
bool MathStructure::isolate_x(const EvaluationOptions &eo, const EvaluationOptions &feo, const MathStructure &x_varp, bool check_result) {
	if(isProtected()) return false;
	if(!isComparison()) {		
		bool b = false;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isolate_x(eo, feo, x_varp, check_result)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
		return b;
	}

	MathStructure x_var(x_varp);
	if(x_var.isUndefined()) {
		const MathStructure *x_var2;
		if(eo.isolate_var && contains(*eo.isolate_var)) x_var2 = eo.isolate_var;
		else x_var2 = &find_x_var();
		if(x_var2->isUndefined()) return false;
		x_var = *x_var2;
	}

	if(CHILD(0) == x_var && !CHILD(1).contains(x_var)) return true;
	if(!CHILD(1).isZero()) {
		CHILD(0).calculateSubtract(CHILD(1), eo);
		CHILD(1).clear(true);
		CHILDREN_UPDATED
	}

	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.test_comparisons = false;
	eo2.isolate_x = false;
	if(!check_result) {
		return isolate_x_sub(feo, eo2, x_var);
	}

	if(CHILD(1).isZero() && CHILD(0).isAddition()) {
		bool found_1x = false;
		for(size_t i = 0; i < CHILD(0).size(); i++) {
			if(CHILD(0)[i] == x_var) {
				found_1x = true;
			} else if(CHILD(0)[i].contains(x_var)) {
				found_1x = false;
				break;
			}
		}
		if(found_1x) return isolate_x_sub(feo, eo2, x_var);
	}

	MathStructure msave(*this);
	bool b = isolate_x_sub(feo, eo2, x_var);
	if(b) {
		b = test_comparisons(msave, *this, x_var, eo) >= 0;
	}

	return b;
	
}

bool MathStructure::isRationalPolynomial(bool allow_non_rational_coefficient, bool allow_interval_coefficient) const {
	switch(m_type) {
		case STRUCT_NUMBER: {
			if(allow_interval_coefficient) return o_number.isReal() && o_number.isNonZero();
			if(allow_non_rational_coefficient) return o_number.isReal() && !o_number.isInterval() && o_number.isNonZero();
			return o_number.isRational() && !o_number.isZero();
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isAddition() || CHILD(i).isMultiplication() || !CHILD(i).isRationalPolynomial(allow_non_rational_coefficient, allow_interval_coefficient)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isAddition() || !CHILD(i).isRationalPolynomial(allow_non_rational_coefficient, allow_interval_coefficient)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(1).isInteger() && CHILD(1).number().isNonNegative() && !CHILD(1).number().isOne() && !CHILD(0).isMultiplication() && !CHILD(0).isAddition() && !CHILD(0).isPower() && CHILD(0).isRationalPolynomial(allow_non_rational_coefficient, allow_interval_coefficient);
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_interval || containsInterval()) return false;
		}
		case STRUCT_UNIT: {}
		case STRUCT_VARIABLE: {}
		case STRUCT_SYMBOLIC: {
			return representsNonMatrix() && !representsUndefined(true, true);
		}
		default: {}
	}
	return false;
}
const Number &MathStructure::overallCoefficient() const {
	switch(m_type) {
		case STRUCT_NUMBER: {
			return o_number;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isNumber()) {
					return CHILD(i).number();
				}
			}
			return nr_one;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isNumber()) {
					return CHILD(i).number();
				}
			}
			return nr_zero;
		}
		case STRUCT_POWER: {
			return nr_zero;
		}
		default: {}
	}
	return nr_zero;
}

