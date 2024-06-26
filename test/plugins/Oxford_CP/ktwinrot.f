      subroutine ktwinrot(nTwin,TwinRot)
      
      implicit none      
      integer :: i,j,k
      integer,parameter :: m = 3
      integer,intent(in):: nTwin
      
      real*8,intent(out):: TwinRot(nTwin,m,m)
      
      real(kind=8):: dtwindir(nTwin,m),dtwinnor(nTwin,m)

	  dtwindir(1,:) = (/0.82482,-0.56540,0.0/)
	  dtwindir(2,:) = (/-0.82482,-0.56540,0.0/)
	  
! twin normals (K_1) of alpha-Uranium
! see Zhou, Xiao, Wang et al. 2016 (Table 2)
      dtwinnor(1,:) = (/0.56540,0.82482,0.0/) ! eta_1 = [3-10] -> [3a,-b,0], K_1 = (130) -> normal is [b,3a,0]
      dtwinnor(2,:) = (/-0.56540,0.82482,0.0/) ! eta_1 = [-3-10] -> [-3a,-b,0], K_1 = (-130) -> normal is [-b,3a,0]

      ! for compound twins
      ! Q_{ij} = 2 n_i n_j - delta_{ij}
      ! where n_i is the twin plane normal
      ! see Franz Roters' thesis 2011
      ! Advanced Material Models for the Crystal Plasticity Finite Element Method 
      ! equation 7.48
      DO k=1,nTwin ! only compound twins are considered
        DO i=1,m
          DO j=1,m
	    TwinRot(k,i,j) = 2.0 * dtwinnor(k,i) * dtwinnor(k,j)
          END DO
        END DO
      END DO  

      ! for 2nd kind twins
      ! Q_{ij} = 2 d_i d_j - delta_{ij}
      ! where d_i is the twin direction
      ! see Cahn, Acta Mater. 1953 (Figure 2)
      !DO k=3,nTwin
      !  DO i=1,m
      !    DO j=1,m
      !      TwinRot(k,i,j) = 2.0 * dtwindir(k,i) * dtwindir(k,j)
      !    END DO
      !  END DO
      !END DO  

      ! subtract the identity matrix
      ! for both 1st and 2nd kind twin
      DO k=1,nTwin
        DO i=1,m
	  TwinRot(k,i,i) = TwinRot(k,i,i) - 1.0
        END DO
      END DO
      
      return
      end 

