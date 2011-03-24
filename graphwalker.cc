
extern "C" {
#define PERL_NO_GET_CONTEXT /* we want efficiency! */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

}

#include "tbb.h"


/* this is a recursive clone, limited to the core Perl types; similar
 * to threads::shared::shared_clone, except:
 *
 * - clones from another interpreter; should be safe, so long as the
 *   other interpreter is not changing the marshalled data.  In
 *   particular, hashes are not iterated using hv_iterinit etc; this
 *   code is quite tied to the particular Perl API, until there is a
 *   better C-level iterator API for hashes.
 *
 * - implemented using a stack and map; but the approach is quite
 *   similar to the one used in shared_clone's private closure.
 */

#include  <list>
#include  <map>

class graph_walker_slot {
public:
	SV* tsv;
	bool built;
	graph_walker_slot() {};
	graph_walker_slot( SV* tsv, int built = false )
		: tsv(tsv), built(built) { };
};

SV* clone_other_sv(PerlInterpreter* my_perl, SV* sv, PerlInterpreter* other_perl) {

	std::list<SV*> todo;
	std::map<SV*, graph_walker_slot> done;
	std::map<SV*, graph_walker_slot>::iterator item, target;

	todo.push_back(sv);

	// undef is a second-class global.  map it.
	done[&other_perl->Isv_undef] = graph_walker_slot( &PL_sv_undef, true );

	SV* it;
	while (todo.size()) {
		it = todo.back();
		IF_DEBUG_CLONE("cloning %x", it);
		todo.pop_back();
		item = done.find( it );
		bool isnew = (item == done.end());
		if (!isnew && (*item).second.built) {
			// seen before.
			IF_DEBUG_CLONE("   seen, built");
			continue;
		}
		if (SvROK(it)) {
			IF_DEBUG_CLONE("   SV is ROK (%s)", sv_reftype(it,0));
			if (isnew) {
				// '0' means that the item is on todo
				done[it] = graph_walker_slot(newSV(0));
				IF_DEBUG_CLONE("   SV is new");
			}

			// fixme: $x = \$x circular refs
			target = done.find( SvRV(it) );
			if (target == done.end()) {
				// nothing, so remember to init self later.
				todo.push_back(it);
				todo.push_back(SvRV(it));
			}
			else {
				// target exists!  set the ref
				SvUPGRADE((*item).second.tsv, SVt_RV);
				SvRV_set((*item).second.tsv, (*target).second.tsv);
				SvREFCNT_inc((*target).second.tsv);

				// and here we bless things
				if (SvOBJECT(SvRV(it))) {
					HV* pkg = SvSTASH(SvRV(it));
					target = done.find((SV*)pkg);
					if (target == done.end()) {
						char * const name = HvNAME_get(pkg);
						//HV* lpkg = gv_stashpvs(name, 1);
						//done[(SV*)pkg] = graph_walker_slot((SV*)lpkg, true);
						//sv_bless( (*item).second.tsv, lpkg );
					}
					else {
						sv_bless( (*item).second.tsv, (HV*) (*target).second.tsv );
					}
				}
				done[it].built = true;
			}
		}
		else {
			// error: jump to case label crosses initialization of ‘...’
			bool all_found = true;
			int num;
			HE** contents;
			const char* str;
			STRLEN len;
			SV* nv;
			IF_DEBUG_CLONE("   SV is not ROK but type %d", SvTYPE(it));
			switch (SvTYPE(it)) {
			case SVt_PVAV:
				IF_DEBUG_CLONE("     => AV");
				// array ... seen?
				if (isnew) {
					done[it] = graph_walker_slot((SV*)newAV());
				}

				for (int i = 0; i <= av_len((AV*)it); i++ ) {
					SV** slot = av_fetch((AV*)it, i, 0);
					if (!slot)
						continue;

					target = done.find(*slot);
					if (target == done.end()) {
						if (all_found) {
							todo.push_back(it);
							all_found = false;
						}
						todo.push_back(*slot);
					}
				}
				if (all_found) {
					AV* av = (AV*)done[it].tsv;
					av_extend(av, av_len((AV*)it));
					for (int i = 0; i <= av_len((AV*)it); i++ ) {
						SV** slot = av_fetch((AV*)it, i, 0);
						if (!slot)
							continue;
						target = done.find(*slot);
						av_store( av, i, (*target).second.tsv );
					}
					done[it].built = true;
				}
				break;

			case SVt_PVHV:
				IF_DEBUG_CLONE("     => HV");
				// hash
				if (isnew) {
					done[it] = graph_walker_slot((SV*)newHV());
				}

				// side-effect free hash iteration :)
				num = HvMAX(it);
				contents = HvARRAY(it);
				for (int i = 0; i < num; i++ ) {
					SV* val = HeVAL(contents[i]);
					// thankfully, PL_sv_placeholder is a superglobal.
					if (val == &PL_sv_placeholder)
						continue;

					target = done.find(val);
					if (target == done.end()) {
						if (all_found) {
							todo.push_back(it);
							all_found = false;
						}
						todo.push_back(val);
					}
				}
				if (all_found) {
					HV* hv = (HV*)done[it].tsv;
					for (int i = 0; i < num; i++ ) {
						HE* hent = contents[i];
						SV* val = HeVAL( hent );
						if (val == &PL_sv_placeholder)
							continue;
						STRLEN key_len;
						const char* key = HePV( hent, key_len );
						
						target = done.find(val);
						hv_store( hv, key, key_len, (*target).second.tsv, 0 );
					}
					done[it].built = true;
				}
				break;
			case SVt_PVCV:
				// for now, barf.
				croak("cannot put CODE reference in a concurrent container");
				break;
			case SVt_PVGV:
				croak("cannot put GLOB reference in a concurrent container");
				break;
			case SVt_IV:
				IF_DEBUG_CLONE("     => IV (%d)", SvIV(it));
				done[it] = graph_walker_slot(newSViv(SvIV(it)), true);
				break;
			case SVt_NV:
				IF_DEBUG_CLONE("     => NV (%g)", SvNV(it));
				done[it] = graph_walker_slot(newSVnv(SvNV(it)), true);
				break;
			case SVt_PVNV:	
				IF_DEBUG_CLONE("     => PVNV (%s, %g)", SvPV_nolen(it), SvNV(it));
				goto xx;
			case SVt_PVIV:
				IF_DEBUG_CLONE("     => PVIV (%s, %d)", SvPV_nolen(it), SvIV(it));
				goto xx;
		
			case SVt_PV:
				IF_DEBUG_CLONE("     => PV (%s)", SvPV_nolen(it));
			xx:
				STRLEN len;
				str = SvPV(it, len);
				done[it] = graph_walker_slot(newSVpv( str, len ), true);
				break;
			default:
				croak("unknown SV type %d SVt_PVIV = %d; cannot marshall through concurrent container",
				      SvTYPE(it), SVt_PVNV);
			}
		}
	}

	SV* rv = done[sv].tsv;
	IF_DEBUG_CLONE("clone returning %x", rv);
	return rv;
}
