#ifndef _IOMMU_H
#define _IOMMU_H

struct udevice;

#if (CONFIG(OF_CONTROL) && !CONFIG(OF_PLATDATA)) && \
	CONFIG(IOMMU)
int dev_iommu_enable(struct udevice *dev);
#else
static inline int dev_iommu_enable(struct udevice *dev)
{
	return 0;
}
#endif

#endif
