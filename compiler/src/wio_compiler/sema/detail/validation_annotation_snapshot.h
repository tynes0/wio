#pragma once

#include <memory>

namespace wio
{
    struct ASTNode;

    namespace sema::detail
    {
        class ValidationAnnotationSnapshot final
        {
        public:
            ValidationAnnotationSnapshot();
            ~ValidationAnnotationSnapshot();

            ValidationAnnotationSnapshot(const ValidationAnnotationSnapshot&) = delete;
            ValidationAnnotationSnapshot& operator=(const ValidationAnnotationSnapshot&) = delete;

            void capture(ASTNode* root);
            void restore() const;

        private:
            struct State;
            std::unique_ptr<State> state_;
        };
    }
}
