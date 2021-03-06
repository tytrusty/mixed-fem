d=3;
R=sym('R',[d d]);
F=sym('F',[3 3]);
s = sym('S',[6,1]);
sval = sym('sval',[3,1]);
L = sym('L',[9,1]);
syms mu la
assume(R,'real')
assume(F,'real')
assume(s,'real')
assume(mu,'real')
assume(L,'real')
assume(sval,'real')

S = [s(1) s(4) s(5);
     s(4) s(2) s(6);
     s(5) s(6) s(3)];

W = [                          
  [R(1,1) 0 0 0 R(1,3) R(1,2)]
  [0 R(1,2) 0 R(1,3) 0 R(1,1)]
  [0 0 R(1,3) R(1,2) R(1,1) 0]
  [R(2,1) 0 0 0 R(2,3) R(2,2)]
  [0 R(2,2) 0 R(2,3) 0 R(2,1)]
  [0 0 R(2,3) R(2,2) R(2,1) 0]
  [R(3,1) 0 0 0 R(3,3) R(3,2)]
  [0 R(3,2) 0 R(3,3) 0 R(3,1)]
  [0 0 R(3,3) R(3,2) R(3,1) 0]
  ];

% stable neohookean
I3=det(F);
I2=trace(F'*F);
I3=det(S);
I2=trace(S'*S);
I2 = sum(sval.^2);
I3 = prod(sval);


snh= 0.5*mu*(I2-d)- mu*(I3-1)+ 0.5*la*(I3-1)^2;
H=simplify(hessian(snh,sval(:)));
g=simplify(gradient(snh,sval(:)));
ccode(snh)
ccode(H)
ccode(g)

% neohookean
%nh = 0.5*mu*(I2/(I3^(2/3)) - 3) + 0.5*la*(I3-1)^2;
nh = 0.5*mu*(I2- 3) - mu*log(I3) + 0.5*la*(log(I3))^2;
% H=(hessian(nh,s(:)));
% g=gradient(nh,s(:));
H=(hessian(nh,F(:)));
g=gradient(nh,F(:));
ccode(nh)
ccode(H)
ccode(g)

%F=R*S;
%J=det(F);
%I3=trace(F'*F)/J^(2/3);
%snh=0.5*mu*(I3-3)+ 0.5*la*(J-1)^2;

% Corotational material model
arap= mu*0.5*trace( (S - eye(d))*(S - eye(d))');
H=hessian(arap,s(:));
g=gradient(arap,s(:));
ccode(arap)
ccode(H)
ccode(g)

corot = la*0.5*trace(S-eye(d))^2 + 2*arap;
H=hessian(corot,s(:));
g=gradient(corot,s(:));
ccode(corot)
ccode((H))
ccode(g)
