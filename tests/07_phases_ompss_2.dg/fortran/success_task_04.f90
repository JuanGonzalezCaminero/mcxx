! <testinfo>
! test_generator=config/mercurium-ompss-2
! </testinfo>



!! In OmpSs-2, the data environment of a task is destroyed once all its children tasks have been executed-

SUBROUTINE FOO(M)
    IMPLICIT NONE
    INTEGER :: M
    INTEGER :: I
    INTEGER :: V(10)
    INTEGER :: W(M)
    INTEGER, ALLOCATABLE :: X(:)

    ALLOCATE(X(10))

    !$OSS TASK PRIVATE(I, V, W, X)
        !$OSS TASK SHARED(I, V, W, X)
            I=0
            V=0
            W=0
            !! Uncomment the following line  once #2829 is fixed!
            !! X=0
        !$OSS END TASK
    !$OSS END TASK

    !$OSS TASKWAIT
END SUBROUTINE FOO

PROGRAM P
    IMPLICIT NONE
    CALL FOO(10)
END PROGRAM P
