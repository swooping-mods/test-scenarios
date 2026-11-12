/* A capsule fixture owns its authored records, never an existing world's cargo. */
typedef struct {
    DRoot world,actor,original,authored,record,view;
    int original_next,id,mutated,validated,physical_pose,uncertain;
    DVec position;
    double kilograms;
} CFixture;
