#ifndef __IMX_VPU_H__
#define __IMX_VPU_H__ "$Id$"

/*
 * imx_vpu.h
 *
 * This header file declares the imx_vpu_t class, which
 * takes care of initializing the vpu library.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

class vpu_t {
public:
	vpu_t(void);
	~vpu_t(void);

	bool worked(void) const { return worked_ ; }
private:
	bool worked_ ;
};

#endif
