
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
		//IF_DEBUG_CLONE("   SV is %s", (isnew ? "new" : "repeat"));
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
				IF_DEBUG_CLONE("   refers to unseen ref");
				// nothing, so remember to init self later.
				todo.push_back(it);
				todo.push_back(SvRV(it));
			}
			else {
				IF_DEBUG_CLONE("   refers to seen ref");
				// target exists!  set the ref
				SvUPGRADE((*item).second.tsv, SVt_RV);
				SvRV_set((*item).second.tsv, (*target).second.tsv);
				SvROK_on((*item).second.tsv);
				SvREFCNT_inc((*target).second.tsv);
				sv_2mortal((*item).second.tsv);
				
				IF_DEBUG_CLONE("   %x now refers to %x: ",
					       (*item).second.tsv,
					       (*target).second.tsv
					);

				// and here we bless things
				if (SvOBJECT(SvRV(it))) {
					HV* pkg = SvSTASH(SvRV(it));
					target = done.find((SV*)pkg);
					if (target == done.end()) {
						const char * pkgname = HvNAME_get(pkg);
						HV* lpkg = gv_stashpv(pkgname, GV_ADD);
						done[(SV*)pkg] = graph_walker_slot((SV*)lpkg, true);
						sv_bless( (*item).second.tsv, lpkg );
					}
					else {
						sv_bless( (*item).second.tsv, (HV*) (*target).second.tsv );
					}
					IF_DEBUG_CLONE("    blessed be! => %s", HvNAME_get(pkg));
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
					IF_DEBUG_CLONE("   new AV");
					done[it] = graph_walker_slot((SV*)newAV());
				}

				for (int i = 0; i <= av_len((AV*)it); i++ ) {
					SV** slot = av_fetch((AV*)it, i, 0);
					if (!slot)
						continue;

					target = done.find(*slot);
					if (target == done.end()) {
						if (all_found) {
							IF_DEBUG_CLONE("   contains unseen slot values");
							todo.push_back(it);
							all_found = false;
						}
						todo.push_back(*slot);
					}
				}
				if (all_found) {
					IF_DEBUG_CLONE("   no unseen slot values");
					AV* av = (AV*)done[it].tsv;
					IF_DEBUG_CLONE("   unshift av, %d", av_len((AV*)it)+1);
					av_unshift(av, av_len((AV*)it)+1);
					for (int i = 0; i <= av_len((AV*)it); i++ ) {
						SV** slot = av_fetch((AV*)it, i, 0);
						if (!slot)
							continue;
						SV* targsv = done[*slot].tsv;
						SV** slot2 = av_fetch( av, i, 1 );
						*slot2 = targsv;
						SvREFCNT_inc(targsv);
						IF_DEBUG_CLONE("      slot[%d] = %x (refcnt = %d, type = %d, pok = %d, iok = %d)", i, targsv, SvREFCNT(targsv), SvTYPE(targsv), SvPOK(targsv)?1:0, SvIOK(targsv)?1:0);
					}
					sv_2mortal((SV*)av);
					done[it].built = true;
				}
				break;

			case SVt_PVHV:
				IF_DEBUG_CLONE("     => HV");
				// hash
				if (isnew) {
					IF_DEBUG_CLONE("   new HV");
					done[it] = graph_walker_slot((SV*)newHV());
				}

				// side-effect free hash iteration :)
				num = HvMAX(it);
				contents = HvARRAY(it);
				IF_DEBUG_CLONE("   walking over %d slots at contents @%x", num+1, contents);
				IF_DEBUG_CLONE("   (PL_sv_placeholder = %x)", &PL_sv_placeholder);
				for (int i = 0; i <= num; i++ ) {
					IF_DEBUG_CLONE("   contents[%d] = %x", i, contents[i]);
					if (!contents[i])
						continue;
					SV* val = HeVAL(contents[i]);
					IF_DEBUG_CLONE("   val = %x", val);
					// thankfully, PL_sv_placeholder is a superglobal.
					if (val == &PL_sv_placeholder)
						continue;

					target = done.find(val);
					if (target == done.end()) {
						if (all_found) {
							IF_DEBUG_CLONE("   contains unseen slot values");
							todo.push_back(it);
							all_found = false;
						}
						todo.push_back(val);
					}
				}
				if (all_found) {
					IF_DEBUG_CLONE("   no unseen slot values");
					HV* hv = (HV*)done[it].tsv;
					for (int i = 0; i <= num; i++ ) {
						HE* hent = contents[i];
						if (!hent) {
							continue;
						}
						SV* val = HeVAL( hent );
						if (val == &PL_sv_placeholder)
							continue;
						STRLEN key_len;
						const char* key = HePV( hent, key_len );
						
						target = done.find(val);
						IF_DEBUG_CLONE("   hv_fetch(%x, '%s', %d, 1)", hv, key, key_len);
						SV**slot = hv_fetch( hv, key, key_len, 1); 
						IF_DEBUG_CLONE("   => %x", done[val].tsv);
						*slot = done[val].tsv;
						SvREFCNT_inc(*slot);
						//hv_store( hv, key, key_len, (*target).second.tsv, 0 );
					}
					sv_2mortal((SV*)hv);
					//SvREFCNT_inc((SV*)hv);
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
				done[it] = graph_walker_slot(sv_2mortal(newSViv(SvIV(it))), true);
				break;
			case SVt_NV:
				IF_DEBUG_CLONE("     => NV (%g)", SvNV(it));
				done[it] = graph_walker_slot(sv_2mortal(newSVnv(SvNV(it))), true);
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
				done[it] = graph_walker_slot(sv_2mortal(newSVpv( str, len )), true);
				break;
			default:
				croak("unknown SV type %d SVt_PVIV = %d; cannot marshall through concurrent container",
				      SvTYPE(it), SVt_PVNV);
			}
			IF_DEBUG_CLONE("cloned %x => %x / t=%d / rc=%d", it, done[it].tsv, SvTYPE(done[it].tsv), SvREFCNT(done[it].tsv));
		}
	}

	SV* rv = done[sv].tsv;
	IF_DEBUG_CLONE("clone returning %x", rv);
	return rv;
}

SV* perl_concurrent_slot::dup( pTHX ) {
	SV* rsv;
	if (this->owner == my_perl) {
		rsv = newSV(0);
		SvSetSV_nosteal(rsv, this->thingy);
	}
	else {
		IF_DEBUG_CLONE("CLONING %x (refcnt = %d)", this->thingy, SvREFCNT(this->thingy));
		rsv = clone_other_sv( my_perl, this->thingy, this->owner );
		SvREFCNT_inc(rsv);
	}
	return rsv;
}
