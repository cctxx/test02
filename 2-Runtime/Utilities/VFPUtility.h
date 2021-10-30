#ifndef VFPTILITY_H
#define VFPTILITY_H

#if UNITY_SUPPORTS_VFP

#define FMULS3(s0,s1,s2, s4,s5,s6, s8,s9,s10) \
	fmuls s##s0, s##s4, s##s8 ; \
	fmuls s##s1, s##s5, s##s9 ; \
	fmuls s##s2, s##s6, s##s10

#define FMACS3(s0,s1,s2, s4,s5,s6, s8,s9,s10) \
	fmacs s##s0, s##s4, s##s8 ; \
	fmacs s##s1, s##s5, s##s9 ; \
	fmacs s##s2, s##s6, s##s10

#define FCPYS3(s0,s1,s2, s4,s5,s6) \
	fcpys s##s0, s##s4 ; \
	fcpys s##s1, s##s5 ; \
	fcpys s##s2, s##s6 ; \



#define FMULS4(s0,s1,s2,s3, s4,s5,s6,s7, s8,s9,s10,s11) \
	fmuls s##s0, s##s4, s##s8 ; \
	fmuls s##s1, s##s5, s##s9 ; \
	fmuls s##s2, s##s6, s##s10 ;\
	fmuls s##s3, s##s7, s##s11 

#define FMACS4(s0,s1,s2,s3, s4,s5,s6,s7, s8,s9,s10,s11) \
	fmacs s##s0, s##s4, s##s8 ; \
	fmacs s##s1, s##s5, s##s9 ; \
	fmacs s##s2, s##s6, s##s10 ;\
	fmacs s##s3, s##s7, s##s11 

#define FCPYS4(s0,s1,s2,s3, s4,s5,s6,s7) \
	fcpys s##s0, s##s4 ; \
	fcpys s##s1, s##s5 ; \
	fcpys s##s2, s##s6 ; \
	fcpys s##s3, s##s7 ; \

#endif

#endif
