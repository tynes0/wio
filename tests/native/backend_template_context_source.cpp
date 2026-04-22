template <typename T>
void backendTemplateContextLeaf()
{
    typename T::missing_type broken{};
    (void)broken;
}

template <typename T>
void backendTemplateContextMid()
{
    backendTemplateContextLeaf<T>();
}

void backendTemplateContextTrigger()
{
    backendTemplateContextMid<int>();
}
