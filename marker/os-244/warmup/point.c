#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void
point_translate(struct point *p, double x, double y)
{
	p->x += x;
	p->y += y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	// if (!p) return -1.0;
	return sqrt(pow(p1->x - p2->x, 2) + pow(p1->y - p2->y, 2));
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	struct point origin;
	point_set(&origin, 0, 0);
	double d1 = point_distance(p1, &origin);
	double d2 = point_distance(p2, &origin);
	if (d1 < d2)
		return -1;
	else if (d1 == d2)
		return 0;
	else 
		return 1;
}
