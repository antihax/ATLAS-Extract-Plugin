

\W([a-zA-Z-_<>]+) (.*)?;


$1& $2Field() { return *GetNativePointerField<$1*>(this, "APrimalModularShip.$2"); }