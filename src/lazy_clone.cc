
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
 *   better C-level (const) iterator API for hashes.
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
	MAGIC* mg;
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
		// mg_find is a pTHX_ function but should be OK...
		if (mg = SvTIED_mg(it, PERL_MAGIC_tied)) {
			IF_DEBUG_CLONE("   SV is TIED %s", sv_reftype(it,0));
			if (isnew) {
				done[it] = graph_walker_slot(newSV(0));
				IF_DEBUG_CLONE("   SV is new");
			}
			if ( ! mg->mg_obj ) {
				croak("no magic object to be found for tied %s at %x", sv_reftype(it,0), it);
			}
			target = done.find( mg->mg_obj );
			if ( target == done.end() ) {
				IF_DEBUG_CLONE("   tied SV (%x) unseen", mg->mg_obj);
				todo.push_back(it);
				todo.push_back(mg->mg_obj);
			}
			else {
				SV* obj = (*item).second.tsv;
				SV* mg_obj = (*target).second.tsv;
				IF_DEBUG_CLONE("   upgrading %x to type %d", obj, SvTYPE(it));
				SvUPGRADE(obj, SvTYPE(it));
				sv_magic(obj, mg_obj, PERL_MAGIC_tied, 0, 0);
				IF_DEBUG_CLONE("   made magic: %x (rc=%d)", mg_obj, SvREFCNT(mg_obj));
				done[it].built = true;
			}
		}
		else if (SvROK(it)) {
			IF_DEBUG_CLONE("   SV is ROK (%s)", sv_reftype(it,0));
			if (SvOBJECT(SvRV(it))) {
				IF_DEBUG_CLONE("     In fact, it's blessed");
				// should first check whether the type gets cloned at all...
				HV* pkg = SvSTASH(SvRV(it));
				target = done.find((SV*)pkg);
				if (target == done.end()) {
					const char * pkgname = HvNAME_get(pkg);
					IF_DEBUG_CLONE("     Ok, %s, that's new", pkgname);
					HV* lpkg = gv_stashpv(pkgname, GV_ADD);
					// not found ... before we map to a local package, call the CLONE_SKIP function to see if we should map this type.
					const HEK * const hvname = HvNAME_HEK(pkg);
					GV* const cloner = gv_fetchmethod_autoload(lpkg, "CLONE_SKIP", 0);
					UV status = 0;
					if (cloner && GvCV(cloner)) {
						IF_DEBUG_CLONE("     Calling CLONE_SKIP in %s", pkgname);
						dSP;
						ENTER;
						SAVETMPS;
						PUSHMARK(SP);
						mXPUSHs(newSVhek(hvname));
						PUTBACK;
						call_sv(MUTABLE_SV(GvCV(cloner)), G_SCALAR);
						SPAGAIN;
						status = POPu;
						IF_DEBUG_CLONE("     CLONE_SKIP returned %d", status);
						PUTBACK;
						FREETMPS;
						LEAVE;
					}
					else {
						IF_DEBUG_CLONE("     No CLONE_SKIP defined in %s", pkgname);
					}
					if (status) {
						IF_DEBUG_CLONE("     marking package (%x) as undef", pkg);
						done[(SV*)pkg] = graph_walker_slot(&PL_sv_undef, true);
						IF_DEBUG_CLONE("     CLONE SKIP set: mapping SV %x to undef", it);
						done[it] = graph_walker_slot(&PL_sv_undef, true);
						continue;
					}
					else {
						done[(SV*)pkg] = graph_walker_slot((SV*)lpkg, true);
						IF_DEBUG_CLONE("     adding package (%x) to done hash (%x)", pkg, lpkg);
					}
					target = done.find((SV*)pkg);
				}
				else {
					if ((*target).second.tsv == &PL_sv_undef) {
						IF_DEBUG_CLONE("     CLONE SKIP previously set: mapping SV to undef");
						continue;
					}
				}
			}
			if (isnew) {
				// '0' means that the item is on todo
				done[it] = graph_walker_slot(newSV(0));
				item = done.find( it );
				IF_DEBUG_CLONE("   SV is new");
			}

			// fixme: $x = \$x circular refs
			target = done.find( SvRV(it) );
			if (target == done.end()) {
				IF_DEBUG_CLONE("   refers to unseen ref %x", SvRV(it));
				// nothing, so remember to init self later.
				todo.push_back(it);
				todo.push_back(SvRV(it));
			}
			else {
				IF_DEBUG_CLONE("   refers to seen ref %x (%x)", SvRV(it), (*target).second.tsv);
				// target exists!  set the ref
				IF_DEBUG_CLONE("   (upgrade %x to RV)", (*item).second.tsv);
				SvUPGRADE((*item).second.tsv, SVt_RV);
				SvRV_set((*item).second.tsv, (*target).second.tsv);
				IF_DEBUG_CLONE("   (set RV targ)");
				SvROK_on((*item).second.tsv);
				IF_DEBUG_CLONE("   (set ROK)");
				SvREFCNT_inc((*target).second.tsv);
				IF_DEBUG_CLONE("   (inc rc to %d)", SvREFCNT((*target).second.tsv));
				(*item).second.tsv;
				
				IF_DEBUG_CLONE("   %x now refers to %x: ",
					       (*item).second.tsv,
					       (*target).second.tsv
					);

				// and here we bless things
				if (SvOBJECT(SvRV(it))) {
					HV* pkg = SvSTASH(SvRV(it));
					target = done.find((SV*)pkg);
					if (target == done.end()) {
						IF_DEBUG_CLONE("     couldn't find package in map :(");
					}
					sv_bless( (*item).second.tsv, (HV*) (*target).second.tsv );
					IF_DEBUG_CLONE("    blessed be! => %s", HvNAME_get(pkg));
					if (SvTYPE(SvRV(it)) == SVt_PVMG) {
						// XS object, it better know how to refcount.
						GV* const rc_inc = gv_fetchmethod_autoload(pkg, "CLONE_REFCNT_inc", 0);
						UV status = -1;
						if (rc_inc && GvCV(rc_inc)) {
							IF_DEBUG_CLONE("     Calling CLONE_REFCNT_inc in %s", HvNAME_get(pkg));
							dSP;
							ENTER;
							SAVETMPS;
							PUSHMARK(SP);
							XPUSHs( (*item).second.tsv );
							PUTBACK;
							call_sv(MUTABLE_SV(GvCV(rc_inc)), G_SCALAR);
							SPAGAIN;
							status = POPu;
							IF_DEBUG_CLONE("     CLONE_REFCNT_inc returned %d", status);
							PUTBACK;
							FREETMPS;
							LEAVE;
						}
						if (status != 42) {
							warn("Leaking memory because XS class %s didn't define CLONE_SKIP nor CLONE_REFCNT_inc", HvNAME_get(pkg));
						}
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
			SV* magic_sv;
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
					(SV*)av;
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
				//IF_DEBUG_CLONE("   walking over %d slots at contents @%x", num+1, contents);
				//IF_DEBUG_CLONE("   (PL_sv_placeholder = %x)", &PL_sv_placeholder);
				for (int i = 0; i <= num; i++ ) {
				  //IF_DEBUG_CLONE("   contents[%d] = %x", i, contents[i]);
					if (!contents[i])
						continue;
					SV* val = HeVAL(contents[i]);
					IF_DEBUG_CLONE("   {%s} = %x", HePV(contents[i], len), val);
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
					(SV*)hv;
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
			case SVt_PVMG:
				IF_DEBUG_CLONE("     => PVMG (%x)", SvIV(it));
				IF_DEBUG_LEAK("new PVMG: %x", SvIV(it));
				done[it] = graph_walker_slot(newSViv(SvIV(it)), true);
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
